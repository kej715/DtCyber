#!/usr/bin/env node
//
// Update PID/LID deefinitions in the LIDCMxx file to reflect RHP, NJE, and TLF
// configuration.
//

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const mid            = utilities.getMachineId(dtc);
const hostID         = utilities.getHostId(dtc);
const njeTopology    = utilities.getNjeTopology(dtc);
const rhpTopology    = utilities.getRhpTopology(dtc);
const tlfTopology    = utilities.getTlfTopology(dtc);

let adjacentNodes    = {};
let nonadjacentNodes = {};

const isCrsInstalled = utilities.isInstalled("crs");
const isNjfInstalled = utilities.isInstalled("njf");
const isRhpInstalled = utilities.isInstalled("rhp");
const isTlfInstalled = utilities.isInstalled("tlf");

const customProps    = utilities.getCustomProperties(dtc);

//
// Update topology and network parameters to reflect customizations, if any.
// Customizations include locally defined NJE and TLF nodes.
//
const mfTypeTable = {
  COS:     "COS",
  HUJINET: "UNIX",
  JES2:    "MVS",
  JNET:    "VAX/VMS",
  NJEF:    "NOS2",
  NJE38:   "MVS",
  NOS:     "NOS2",
  PRIME:   "PRIME",
  RSCS:    "VM/CMS"
};

const getMfType = node => {
  let mfType = "UNKNOWN";
  if (typeof node.software !== "undefined") {
    mfType = mfTypeTable[node.software];
  }
  else if (node.type === "RHP") {
    mfType = "NOS2";
  }
  else if (node.type === "TLF") {
    mfType = mfTypeTable[node.spooler];
  }
  return (typeof mfType !== "undefined") ? mfType : "UNKNOWN";
};

let topology = {};

if (isCrsInstalled) {
  let value = utilities.getPropertyValue(customProps, "NETWORK", "crayStation", null);
  if (value !== null) {
    //
    //  crayStation=<name>,<lid>,<channelNo>,<addr>[,S<station-id>][,C<cray-id>]
    //
    let items = value.split(",");
    if (items.length >= 4) {
      const nodeName = items.shift();
      const node = {
        id: nodeName,
        type: "CRS",
        software: "COS",
        lid: items[0],
        addr: items[2],
        link: hostID
      };
      topology[nodeName] = node;
    }
  }
}

let defaultRoute = null;

if (isTlfInstalled) {
  for (const name of Object.keys(tlfTopology)) {
    let node       = tlfTopology[name];
    node.link      = hostID;
    node.route     = hostID;
    topology[name] = node;
  }
}
if (isNjfInstalled) {
  defaultRoute  = utilities.getDefaultNjeRoute();
  for (const name of Object.keys(njeTopology)) {
    topology[name] = njeTopology[name];
  }
}
if (isRhpInstalled) {
  for (const name of Object.keys(rhpTopology)) {
    if (name === hostID) continue;
    let node = rhpTopology[name];
    if (typeof topology[name] !== "undefined" && topology[name].type === "NJE" && node.lid === topology[name].lid) {
      node.njeLid = `N${node.lid.substring(1)}`;
    }
    topology[name] = node;
  }
}
//
// Verify that the local node is defined in the topology, and finalize
// the default route.
//
if (typeof topology[hostID] === "undefined") {
  topology[hostID] = {
    id: hostID,
    type: "RHP",
    software: "NOS",
    lid: `M${mid}`
  };
  if (defaultRoute !== null) topology[hostID].link = defaultRoute;
}
let localNode = topology[hostID];
if (defaultRoute === null) {
  if (typeof localNode.link !== "undefined") {
    defaultRoute = localNode.link;
  }
  else {
    defaultRoute = hostID;
  }
}

//
//  Get explicitly defined store-and-forward nodes, if any.
//  
let safNodes = {};

if (typeof customProps["NETWORK"] !== "undefined") {
  for (let line of customProps["NETWORK"]) {
    line = line.toUpperCase();
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim();
    let value = line.substring(ei + 1).trim();
    if (key === "SAFNODE") {
      //
      //  safNode=<host-id>,<relay-host-id>,<lid>
      //
      let items = value.split(",");
      if (items.length >= 3) {
        let node = {
          id: items.shift(),
          relayHost: items.shift(),
          lid: items.shift()
        };
        safNodes[node.id] = node;
      }
    }
  }
}

//
// Build tables of adjacent and non-adjacent nodes with LIDs
//
if (typeof localNode.link !== "undefined"
    && typeof topology[localNode.link] != "undefined"
    && localNode.link !== hostID) {
  let linkNode = topology[localNode.link];
  linkNode.mfType = getMfType(linkNode);
  adjacentNodes[linkNode.id] = linkNode;
}
let nodeNames = Object.keys(topology);
for (const nodeName of nodeNames) {
  let node = topology[nodeName];
  if (node.id !== hostID) {
    if (node.link === hostID
        || (typeof node.links !== "undefined" && typeof node.links[hostID] !== "undefined")) {
      node.mfType = getMfType(node);
      adjacentNodes[nodeName] = node;
    }
    else if (typeof safNodes[node.id] === "undefined"
             && typeof node.lid !== "undefined"
             && node.id !== localNode.link) {
      if (typeof node.route === "undefined") {
        node.route = defaultRoute;
      }
      nonadjacentNodes[nodeName] = node;
    }
  }
}

let lidConf = {};

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say(`Create/update LIDCM${mid} ...`))
.then(() => {
  const myPid = `M${mid}`;
  //
  // Create/update PID and LID definitions from the lists of adjacent
  // and nonadjacent nodes.
  //
  let localPid = {
    comments: ["*", `* ${hostID} - LOCAL NODE`, "*"],
    mfType: "NOS2",
    lids: {}
  };
  localPid.lids[myPid] = `NLID,LID=${myPid}.`;
  lidConf[myPid]       = localPid;
  for (const node of Object.values(adjacentNodes)) {
    let lid = node.lid;
    if (node.type !== "CRS" && node.type !== "RHP") {
      localPid.lids[lid] = `NLID,LID=${lid},AT=STOREF.`;
    }
    let adjacentPid = {
      comments: ["*", `* ${node.id} - ADJACENT ${(typeof node.type !== "undefined") ? node.type : "NJE"} NODE`, "*"],
      mfType: node.mfType,
      lids: {}
    };
    adjacentPid.lids[lid] = `NLID,LID=${lid}.`;
    if (typeof node.njeLid !== "undefined") {
      adjacentPid.lids[node.njeLid] = `NLID,LID=${node.njeLid},ENABLED=NO.`;
    }
    lidConf[lid] = adjacentPid;
  }
  for (const node of Object.values(nonadjacentNodes)) {
    if (typeof node.route === "undefined" || node.route === null) continue;
    let lid = node.lid;
    if (node.route !== hostID) {
      let adjacentPid = lidConf[adjacentNodes[node.route].lid];
      adjacentPid.lids[lid] = `NLID,LID=${lid}.`;
      if (typeof node.njeLid !== "undefined") {
        adjacentPid.lids[node.njeLid] = `NLID,LID=${node.njeLid}.`;
      }
    }
    localPid.lids[lid]    = `NLID,LID=${lid},AT=STOREF.`;
  }
  for (const node of Object.values(safNodes)) {
    if (typeof adjacentNodes[node.relayHost] !== "undefined") {
      let lid = node.lid;
      let adjacentPid = lidConf[adjacentNodes[node.relayHost].lid];
      adjacentPid.lids[lid] = `NLID,LID=${lid}.`;
      localPid.lids[lid]    = `NLID,LID=${lid},AT=STOREF.`;
    }
  }
  //
  // Create new/updated LIDCMid file
  //
  let promise = Promise.resolve();
  let lidText = [
    `LIDCM${mid}`
  ];
  const pids = Object.keys(lidConf).sort();
  for (const pid of pids) {
    let pidDefn = lidConf[pid];
    if (pid === "NVE" && pidDefn.mfType === "NOSVE") continue; // remove superfluous NVE definition
    if (typeof pidDefn.comments !== "undefined" && pidDefn.comments.length > 0) {
      lidText = lidText.concat(pidDefn.comments);
    }
    lidText.push(`NPID,PID=${pid},MFTYPE=${pidDefn.mfType}.`);
    promise = promise
    .then(() => dtc.say(`  PID=${pid},MFTYPE=${pidDefn.mfType}.`));
    let lids = Object.keys(pidDefn.lids).sort();
    for (const lid of lids) {
      let lidDefn = pidDefn.lids[lid];
      lidText.push(lidDefn);
      promise = promise
      .then(() => dtc.say(`    ${lidDefn.substring(5)}`));
    }
  }
  lidText.push("");
  lidText = lidText.join("\n");
  const fileName = `LIDCM${mid}.txt`;
  //
  // If a previously created LIDCMxx file exists, and it is less than an hour old,
  // and its contents are identical to the newly generated text, then assume that
  // it does not need to be saved on the host again. For example, this can happen
  // when reconfigure.js is run and more than one of NJF, RHP, or TLF are installed.
  //
  if (fs.existsSync(fileName)) {
    const stat = fs.statSync(fileName);
    if (Date.now() - stat.ctimeMs < (60 * 60 * 1000)
        && fs.readFileSync(fileName, "utf8") === lidText) {
      return promise
      .then(() => dtc.say(`LIDCM${mid} is already up to date`));
    }
  }
  let job = [
    `$COPY,INPUT,LIDCM${mid}.`,
    `$REPLACE,LIDCM${mid}.`
  ];
  let options = {
    jobname: "REPLIDC",
    username: "INSTALL",
    password: utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL"),
    data: lidText
  };
  return promise
  .then(() => dtc.createJobWithOutput(12, 4, job, options))
  .then(() => dtc.say(`Move LIDCM${mid} to SYSTEMX ...`))
  .then(() => dtc.dis([
    `GET,LIDCM${mid}.`,
    `PURGE,LIDCM${mid}.`,
    "SUI,377777.",
    `REPLACE,LIDCM${mid}.`,
    `PERMIT,LIDCM${mid},INSTALL=W.`
  ], "MOVELID", 1))
  .then(() => {
    fs.writeFileSync(fileName, lidText);
    return Promise.resolve();
  })
  .then(() => dtc.say(`LIDCM${mid} updated successfully`))
})
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  if (fs.existsSync(`LIDCM${mid}.txt`)) fs.unlinkSync(`LIDCM${mid}.txt`);
  process.exit(1);
});
