#!/usr/bin/env node
//
// Create the NJF configuration including:
//
//  1. NDL modset defining the terminals used by NJF
//  2. The source for the NJF host configuration file
//  3. PID/LID definitions for NJE nodes with LIDs defined
//  4. Update of cyber.ovl file to include terminal
//     definitions for DtCyber
//
// After creating the configuration files:
//  1. The NDL modset is applied and the NDL configuration
//     file is compiled
//  2. The host configuration file is compiled, and the
//     resulting HCFFILE is moved to SYSTEMX.
//  3. The LIDCMid file is updated to add PIDs/LIDs of
//     nodes with LIDs
//

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const customProps = utilities.getCustomProperties(dtc);
const mid         = utilities.getMachineId(dtc);
const hostID      = utilities.getHostId(dtc);
const npuNode     = utilities.getNpuNode(dtc);

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

//
// Read the public network topology definition
//
const topology = JSON.parse(fs.readFileSync("files/nje-topology.json"));

//
// Update topology and network parameters to reflect customizations, if any.
//
let defaultRoute  = null;
let nextPort      = 0x30;
let portCount     = 16;

if (typeof customProps["NETWORK"] !== "undefined") {
  for (let line of customProps["NETWORK"]) {
    line = line.toUpperCase();
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim();
    let value = line.substring(ei + 1).trim();
    if (key === "DEFAULTROUTE") {
      defaultRoute = value;
    }
    else if (key === "NJENODE") {
      //
      //  njeNode=<nodename>,<software>,<lid>,<public-addr>,<link>
      //     [,<local-address>][,B<block-size>][,P<ping-interval>][,<mailer-address>]
      let items = value.split(",");
      if (items.length >= 5) {
        let nodeName = items.shift();
        let node = {
          software: items.shift(),
          lid: items.shift(),
          publicAddress: items.shift(),
          link: items.shift()
        };
        if (items.length > 0) {
          node.localAddress = items.shift();
        }
        while (items.length > 0) {
          let item = items.shift();
          if (item.startsWith("B")) {
            node.blockSize = parseInt(item.substring(1));
          }
          else if (item.startsWith("P")) {
            node.pingInterval = parseInt(item.substring(1));
          }
          else if (item.indexOf("@") > 0) {
            node.mailer = item;
          }
        }
        topology[nodeName] = node;
      }
    }
    else if (key === "NJEPORTS") {
      let items = value.split(",");
      if (items.length > 0) nextPort  = parseInt(items.shift());
      if (items.length > 0) portCount = parseInt(items.shift());
    }
  }
}

//
// Verify that the local node is defined in the topology, and finalize
// the default route.
//
if (typeof topology[hostID] === "undefined") {
  throw new Error(`Local node ${hostID} is undefined`);
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
// Generate an NDL modset. Define a terminal for the local system's default
// route and for each node explicitly linked to the local system.
//
let terminalDefns = [];
let userDefns     = [];
let lineNumber    = 1;
let routingNode   = null;

let adjacentNodes               = {};
let nonadjacentNodesWithLids    = {};
let nonadjacentNodesWithoutLids = {};

const appendTerminalDefn = node => {
  terminalDefns = terminalDefns.concat([
    `*  ${node.id}`,
    `LI${toHex(npuNode)}P${node.claPort}: LINE      PORT=${node.claPort},LTYPE=S2,TIPTYPE=TT13.`,
    `         ${node.terminalName}:  TERMDEV,STIP=USER,TC=TC29,DT=CON,ABL=7,`,
    "                           DBZ=1020,UBZ=11,DBL=7,UBL=12,XBZ=400,",
    `                           HN=${couplerNode},AUTOCON.`
  ]);
};

const calculateRoute = node => {
  if (typeof node.route === "undefined") {
    for (const adjacentNode of Object.values(adjacentNodes)) {
      if (adjacentNode.id === node.link) {
        node.route = adjacentNode.id;
        return node.route;
      }
    }
    if (typeof node.link !== "undefined" && node.link !== hostID) {
      const route = calculateRoute(topology[node.link]);
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

if (typeof topology[hostID].link !== "undefined") {
  routingNode                   = topology[topology[hostID].link];
  const portNum                 = toHex(nextPort);
  routingNode.id                = topology[hostID].link;
  routingNode.terminalName      = `TE${toHex(npuNode)}P${portNum}`;
  routingNode.claPort           = portNum;
  routingNode.lineNumber        = lineNumber++;
  adjacentNodes[routingNode.id] = routingNode;
  appendTerminalDefn(routingNode);
  userDefns.push(`${routingNode.terminalName}: USER,     NJFUSER.`);
  nextPort  += 1;
  portCount -= 1;
  topology[hostID].lineNumber = lineNumber++;
}
for (const key of Object.keys(topology)) {
  let node = topology[key];
  node.id = key;
  if (node.link === hostID) {
    if (portCount < 1) throw new Error("Insufficient number of NJE ports defined");
    adjacentNodes[key] = node;
    const portNum      = toHex(nextPort);
    node.terminalName  = `TE${toHex(npuNode)}P${portNum}`;
    node.claPort       = portNum;
    node.lineNumber    = lineNumber++;
    appendTerminalDefn(node);
    userDefns.push(`${node.terminalName}: USER,     NJFUSER.`);
    nextPort  += 1;
    portCount -= 1;
  }
}
for (const key of Object.keys(topology)) {
  let node = topology[key];
  if (key !== hostID && typeof adjacentNodes[key] === "undefined") {
    const route = calculateRoute(node);
    if (route === null) node.route = defaultRoute;
    if (typeof node.lid !== "undefined" && node.lid !== "" && node.lid !== null) {
      nonadjacentNodesWithLids[key] = node;
    }
    else {
      nonadjacentNodesWithoutLids[key] = node;
    }
  }
}

const ndlModset = [
  "NJF",
  "*IDENT NJF",
  "*DECK NETCFG",
  "*D 123,124"]
  .concat(terminalDefns)
  .concat([
  "*D 229,230"
  ])
  .concat(userDefns)
  .concat("");

fs.writeFileSync("mods/NJF/NDL.mod", ndlModset.join("\n"));

//
// Generate the source for the NJEF host configuration file.
//
let hcfSource     = [];
let nodeNumber    = 1;

//  1. Generate local system's node definition
hcfSource = hcfSource.concat([
  "*",
  "*  LOCAL NJE NODE",
  "*",
  `NODE,${nodeNumber},${hostID},M${mid},,IBM ,NET,JOB.`,
  `OWNNODE,${nodeNumber},CYBER,NETOPS.`
]);
nodeNumber += 1;

//  2. Generate definitions of adjacent nodes
hcfSource = hcfSource.concat([
  "*",
  "*  ADJACENT NJE NODES",
  "*"
]);
let names = Object.keys(adjacentNodes).sort();
for (const name of names) {
  let node = adjacentNodes[name];
  if (typeof node.lid === "undefined" || node.lid === "" || node.lid === null)
    throw new Error(`Adjcent node ${node.id} does not have a LID`);
  node.nodeNumber = nodeNumber++;
  hcfSource.push(
    `NODE,${node.nodeNumber},${node.id},${node.lid},,IBM ,NET,JOB.`
  );
}

//  3. Generate definitions for non-adjacent nodes with LIDs
names = Object.keys(nonadjacentNodesWithLids).sort();
if (names.length > 0) {
  hcfSource = hcfSource.concat([
    "*",
    "*  NON-ADJACENT NJE NODES WITH LIDS",
    "*"
  ]);
  for (const name of names) {
    let node = nonadjacentNodesWithLids[name];
    node.nodeNumber = nodeNumber++;
    hcfSource.push(
      `NODE,${node.nodeNumber},${node.id},${node.lid},,IBM ,NET,JOB.`
    );
  }
  hcfSource = hcfSource.concat([
    "*",
    "*  PATHS TO NON-ADJACENT NJE NODES WITH LIDS",
    "*"
  ]);
  for (const name of names) {
    let node = nonadjacentNodesWithLids[name];
    node.nodeNumber = nodeNumber++;
    hcfSource.push(
      `PATH,${node.id},${node.route}.`
    );
  }
}

//  4. Generate line definitions
hcfSource = hcfSource.concat([
  "*",
  "*  NETWORK LINE DEFINITIONS",
  "*"
]);
names = Object.keys(adjacentNodes).sort();
for (const name of names) {
  let node = adjacentNodes[name];
  hcfSource.push(
    `LNE,${node.lineNumber},${node.terminalName},1000.`
  );
}

//  5. Generate auto-start line definitions
hcfSource = hcfSource.concat([
  "*",
  "*  AUTO-START DIRECTIVES",
  "*"
]);
names = Object.keys(adjacentNodes).sort();
for (const name of names) {
  let node = adjacentNodes[name];
  hcfSource.push(
    `ASLNE,${node.lineNumber},ST1,SR1${node.software === "NJEF" ? ",JT1,JR1" : ""}.`
  );
}

//  6. Generate FAMILY sattements for routing to non-adjacent nodes without LIDs.
//     Associate each non-adjacent node with the closest adjacent node through which
//     the non-adjacent node may be reached.
names = Object.keys(nonadjacentNodesWithoutLids).sort();
if (names.length > 0) {
  hcfSource = hcfSource.concat([
    "*",
    "*  ROUTES TO NON-ADJACENT NODES WITHOUT LIDS",
    "*"
  ]);
  let n = 1;
  for (const name of names) {
    let node = nonadjacentNodesWithoutLids[name];
    hcfSource.push(
      `FAMILY,${node.id},NET${n++},${topology[node.route].lid},${node.route}.`
    );
  }
}
hcfSource.push("");

//
// If an overlay file exists, read it, and merge its definitions with
// definitions generated from customization properties and adjacent node
// definitions, and create a new overlay file.
//
let ovlProps = {};
if (fs.existsSync("cyber.ovl")) {
  dtc.readPropertyFile("cyber.ovl", ovlProps);
}
let ovlText = [
  `hostID=${hostID}`,
  `couplerNode=${couplerNode}`,
  `npuNode=${npuNode}`
];
if (typeof ovlProps["npu.nos287"] !== "undefined") {
  for (const line of ovlProps["npu.nos287"]) {
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim().toUpperCase();
    let value = line.substring(ei + 1).trim();
    if (key === "COUPLERNODE" || key === "NPUNODE" || key === "HOSTID") continue;
    if (key !== "TERMINALS" || value.toUpperCase().indexOf(",NJE,") < 0) {
      ovlText.push(line);
    }
  }
}
let defaultLocalIP = null;
if (typeof localNode.publicAddress !== "undefined") {
  let parts = localNode.publicAddress.split(":");
  if (parts[0] !== "0.0.0.0") {
    defaultLocalIP = parts[0];
  }
}
names = Object.keys(adjacentNodes).sort();
for (const name of names) {
  let node       = adjacentNodes[name];
  let localIP    = defaultLocalIP;
  let listenPort = 175;
  if (typeof node.localAddress !== "undefined") {
    let parts = node.localAddress.split(":");
    if (parts.length > 1) {
      localIP = parts[0];
      listenPort = parts[1];
    }
    else {
      localIP = parts[0];
    }
  }
  let termDefn = `terminals=${listenPort},0x${toHex(node.claPort)},1,nje`
    + `,${typeof node.publicAddress !== "undefined" ? node.publicAddress : "0.0.0.0:0"}`
    + `,${node.id}`;
  if (localIP !== null && localIP !== "0.0.0.0")
    termDefn += `,${localIP}`;
  if (typeof node.blockSize !== "undefined")
    termDefn += `,B${node.blockSize}`;
  else
    termDefn += ",B8192";
  if (typeof node.pingInterval !== "undefined")
    termDefn += `,P${node.pingInterval}`;
  ovlText.push(termDefn);
}
ovlProps["npu.nos287"] = ovlText;

let lines = [];
for (const key of Object.keys(ovlProps)) {
  lines.push(`[${key}]`);
  for (const line of ovlProps[key]) {
    lines.push(`${line}`);
  }
}
lines.push("");

fs.writeFileSync("cyber.ovl", lines.join("\n"));

//
// Utility function for saving/replacing a file on the local system
//
const replaceFile = (filename, data, options) => {
  let job = [
    `$COPY,INPUT,FILE.`,
    `$REPLACE,FILE=${filename}.`
  ];
  if (typeof options === "undefined") options = {};
  if (typeof options.jobname === "undefined") options.jobname = "REPFILE";
  if (typeof options.username !== "INSTALL") job.push(`$PERMIT,${filename},INSTALL=W.`);
  options.data = data;
  return dtc.createJobWithOutput(12, 4, job, options);
};

let lidConf = {};

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Update NDL with NJF terminal definitions ..."))
.then(() => dtc.runJob(12, 4, "opt/njf-ndl.job"))
.then(() => dtc.say("Compile updated NDL ..."))
.then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job", [mid]))
.then(() => dtc.say("Create/update NJF host configuration file ..."))
.then(() => {
  const options = {
    username: "NETADMN",
    password: "NETADMN"
  };
  return replaceFile("HCFSRC", hcfSource.join("\n"), options);
})
.then(() => dtc.say("Compile updated NJF host configuration file ..."))
.then(() => dtc.runJob(12, 4, "opt/njf-compile-hcf.job"))
.then(() => dtc.say("Move NJF host configuration file to SYSTEMX ..."))
.then(() => dtc.dis([
  "GET,HCFFILE.",
  "PURGE,HCFFILE.",
  "SUI,377777.",
  "PURGE,HCFFILE/NA.",
  "DEFINE,HCF=HCFFILE/CT=PU,M=R.",
  "COPY,HCFFILE,HCF.",
  "PERMIT,HCFFILE,INSTALL=W."
], "MOVEHCF", 1))
.then(() => dtc.disconnect())
.then(() => dtc.exec("node", ["lid-configure"]))
.then(() => dtc.say("NJF configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
