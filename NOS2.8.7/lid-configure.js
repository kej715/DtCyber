#!/usr/bin/env node
//
// Update PID/LID deefinitions in the LIDCMxx file to reflect NJE and TLF
// configuration.
//

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const mid            = utilities.getMachineId(dtc);
const hostID         = utilities.getHostId(dtc);
const rhpTopology    = utilities.getRhpTopology(dtc);

let adjacentNodes    = {};
let nonadjacentNodes = {};

const isCrsInstalled = utilities.isInstalled("crs");
const isNjfInstalled = utilities.isInstalled("njf");
const isTlfInstalled = utilities.isInstalled("tlf");

//
// Utility function to calculate the adjacent node nearest to a
// nonadjacent one.
//
const calculateRoute = node => {
  if (typeof node.route === "undefined") {
    for (const adjacentNode of Object.values(adjacentNodes)) {
      if (adjacentNode.id === node.link) {
        node.route = adjacentNode.id;
        return node.route;
      }
    }
    if (typeof node.link !== "undefined" && node.link !== hostID) {
      const route = calculateRoute(njeTopology[node.link]);
      if (route !== null) node.route = route;
      return route;
    }
    else {
      return null;
    }
  }
  else {
    return node.route;
  }
};

//
// Read the public NJE topology definition
//
let njeTopology = isNjfInstalled ? JSON.parse(fs.readFileSync("files/nje-topology.json")) : {};

//
// Update topology and network parameters to reflect customizations, if any.
// Customizations include locally defined NJE and TLF nodes.
//
const mfTypeTable = {
  COS:   "COS",
  JES2:  "MVS",
  JNET:  "VAX/VMS",
  NJEF:  "NOS2",
  NJE38: "MVS",
  NOS:   "NOS2",
  PRIME: "PRIME",
  RSCS:  "VM/CMS"
};

let defaultRoute  = null;

const customProps = utilities.getCustomProperties(dtc);
if (typeof customProps["NETWORK"] !== "undefined") {
  for (let line of customProps["NETWORK"]) {
    line = line.toUpperCase();
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim();
    let value = line.substring(ei + 1).trim();
    if (key === "CRAYSTATION" && isCrsInstalled) {
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
        njeTopology[nodeName] = node;
      }
    }
    else if (key === "DEFAULTROUTE" && isNjfInstalled) {
      defaultRoute = value;
    }
    else if (key === "NJENODE" && isNjfInstalled) {
      //
      //  njeNode=<nodename>,<software>,<lid>,<public-addr>,<link>
      //     [,<local-address>][,B<block-size>][,P<ping-interval>][,<mailer-address>]
      let items = value.split(",");
      if (items.length >= 5) {
        const nodeName = items.shift();
        const node = {
          id: nodeName,
          software: items.shift(),
          lid: items.shift(),
          addr: items.shift(),
          link: items.shift()
        }
        njeTopology[nodeName] = node;
      }
    }
    else if (key === "TLFNODE" && isTlfInstalled) {
      //
      //  tlfNode=<name>,<lid>,<spooler>,<addr>
      //    [,R<remote-id>][,P<password>][,B<block-size>]
      let items = value.split(",");
      if (items.length >= 4) {
        const nodeName = items.shift();
        const node = {
          id: nodeName,
          type: "TLF",
          lid: items.shift(),
          software: items.shift(),
          link: hostID
        };
        njeTopology[nodeName] = node;
      }
    }
  }
}
//
// Verify that the local node is defined in the topology, and finalize
// the default route.
//
if (typeof njeTopology[hostID] === "undefined") {
  njeTopology[hostID] = {
    id: hostID,
    software: "NOS",
    lid: `M${mid}`
  };
  if (defaultRoute !== null) njeTopology[hostID].link = defaultRoute;
}
let localNode = njeTopology[hostID];
if (defaultRoute === null) {
  if (typeof localNode.link !== "undefined") {
    defaultRoute = localNode.link;
  }
  else {
    defaultRoute = hostID;
  }
}

//
// Build tables of adjacent and non-adjacent nodes with LIDs
//
localNode.id = hostID;
if (typeof localNode.link !== "undefined" && typeof njeTopology[localNode.link] != "undefined") {
  let linkNode = njeTopology[localNode.link];
  let mfType = mfTypeTable[linkNode.software];
  if (typeof mfType === "undefined") mfType = "UNKNOWN";
  linkNode.mfType = mfType;
  adjacentNodes[localNode.link] = linkNode;
}

let nodeNames = Object.keys(njeTopology);
for (const nodeName of nodeNames) {
  let node = njeTopology[nodeName];
  node.id = nodeName;
  if (node.link === hostID) {
    let mfType = mfTypeTable[node.software];
    if (typeof mfType === "undefined") mfType = "UNKNOWN";
    node.mfType = mfType;
    adjacentNodes[nodeName] = node;
  }
  else if (typeof node.lid !== "undefined" && nodeName !== hostID) {
    nonadjacentNodes[nodeName] = node;
  }
}

nodeNames = Object.keys(rhpTopology[hostID].links);
for (const nodeName of nodeNames) {
  let node = rhpTopology[nodeName];
  node.id     = nodeName;
  node.type   = "RHP";
  node.mfType = "NOS2";
  adjacentNodes[nodeName] = node;
  if (typeof nonadjacentNodes[nodeName] !== "undefined") {
    delete nonadjacentNodes[nodeName];
  }
}

//
// Calculate routes for nonadjacent nodes
//
for (const node of Object.values(nonadjacentNodes)) {
  node.route = calculateRoute(node);
  if (typeof node.route === "undefined" || node.route === null) {
    node.route = defaultRoute;
  }
}

let lidConf = {};

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say(`Create/update LIDCM${mid} ...`))
.then(() => {
  let job = [
    `$GET,F=LIDCM${mid}/UN=SYSTEMX,NA.`,
    "$IF,.NOT.FILE(F,AS),M01.",
    "$  GET,F=LIDCM01/UN=SYSTEMX.",
    "$ENDIF,M01.",
    "$COPYSBF,F."
  ];
  let options = {jobname:"GETFILE"};
  return dtc.createJobWithOutput(12, 4, job, options);
})
.then(output => {
  let currentPid = null;
  let lines      = output.split("\n").slice(1);
  let comments   = [];
  for (const line of lines) {
    if (line.startsWith("NPID,")) {
      let match = line.match(/NPID,PID=([^,]*),MFTYPE=([^,.]*)/);
      if (match !== null) {
        currentPid = match[1];
        lidConf[currentPid] = {
          comments: comments,
          mfType: match[2],
          lids: {}
        };
        comments = [];
      }
    }
    else if (line.startsWith("NLID,") && currentPid !== null) {
      let match = line.match(/NLID,LID=([^,]*)/);
      if (match !== null) {
        lidConf[currentPid].lids[match[1]] = line;
      }
      comments = [];
    }
    else if (line.startsWith("*")) {
      comments.push(line);
    }
  }
  //
  // Create/update PID and LID definitions from the lists of adjacent
  // and nonadjacent nodes.
  //
  let localPid = {
    comments: ["*", `* ${hostID} - LOCAL NODE`, "*"],
    mfType: "NOS2",
    lids: {}
  };
  localPid.lids[`M${mid}`] = `NLID,LID=M${mid}.`;
  lidConf[`M${mid}`] = localPid;
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
    lidConf[lid] = adjacentPid;
  }
  for (const node of Object.values(nonadjacentNodes)) {
    if (typeof node.route === "undefined" || node.route === null) continue;
    let lid = node.lid;
    let adjacentPid = lidConf[adjacentNodes[node.route].lid];
    adjacentPid.lids[lid] = `NLID,LID=${lid}.`;
    localPid.lids[lid]    = `NLID,LID=${lid},AT=STOREF.`;
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
  // when reconfigure.js is run and both NJF and TLF are installed.
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
    `$COPY,INPUT,FILE.`,
    `$REPLACE,FILE=LIDCM${mid}.`
  ];
  let options = {
    jobname: "REPFILE",
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
