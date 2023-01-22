#!/usr/bin/env node
//
// Create the NJF configuration including:
//
//  1. NDL modset defining the terminals used by NJF
//  2. The source for the NJF host configuration file
//  3. PID/LID definitions for adjacent NJE nodes
//  4. Update of cyber.ovl file to include terminal
//     definitions for DtCyber
//
// After creating the configuration files:
//  1. The NDL modset is applied and the NDL configuration
//     file is compiled
//  2. The host configuration file is compiled, and the
//     resulting HCFFILE is moved to SYSTEMX.
//  3. The LIDCMid file is updated to add PIDs/LIDs of
//     adjacent nodes
//

const fs      = require("fs");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

//
// If a site configuration file exists, and it has a CMRDECK
// section with an MID definition, use that definition.
// Otherwise, use the default machine ID, "01".
//
let customProps = {};
let mid = "01";
dtc.readPropertyFile(customProps);
if (typeof customProps["CMRDECK"] !== "undefined") {
  for (let line of customProps["CMRDECK"]) {
    line = line.toUpperCase();
    let match = line.match(/^MID=([^.]*)/);
    if (match !== null) {
      mid = match[1].trim();
    }
  }
}

//
// Read and parse the cyber.ini file to obtain the coupler and
// NPU node numbers. These will be used in generating NDL
// definitions.
//
let iniProps = {};
dtc.readPropertyFile("cyber.ini", iniProps);
if (fs.existsSync("cyber.ovl")) {
  dtc.readPropertyFile("cyber.ovl", iniProps);
}
let couplerNode = 1;
let npuNode     = 2;
let hostID      = `NCCM${mid}`;
for (let line of iniProps["npu.nos287"]) {
  line = line.toUpperCase();
  let ei = line.indexOf("=");
  if (ei < 0) continue;
  let key   = line.substring(0, ei).trim();
  let value = line.substring(ei + 1).trim();
  if (key === "COUPLERNODE") {
    couplerNode = value;
  }
  else if (key === "NPUNODE") {
    npuNode = value;
  }
  else if (key === "HOSTID") {
    hostID = value;
  }
}

//
// Read the public network topology definition
//
const topology = JSON.parse(fs.readFileSync("files/nje-topology.json"));

//
// Update topology and network parameters to reflect customizations, if any.
//
let defaultRoute  = "NCCMAX";
let nextPort      = 0x30;
let portCount     = 16;

if (typeof customProps["NETWORK"] !== "undefined") {
  for (let line of customProps["NETWORK"]) {
    line = line.toUpperCase();
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim();
    let value = line.substring(ei + 1).trim();
    if (key === "HOSTID") {
      hostID = value;
    }
    else if (key === "COUPLERNODE") {
      couplerNode = value;
    }
    else if (key === "NPUNODE") {
      npuNode = value;
    }
    else if (key === "DEFAULTROUTE") {
      defaultRoute = value;
    }
    else if (key === "NJENODE") {
      //
      //  njeNode=<nodename>,<software>,<lid>,<public-addr>,<link>
      //     [,<local-address>[,B<block-size>][,P<ping-interval>]]
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
// Add a node definition to the topology for the local host, unless it
// is already defined in the topology.
//
if (typeof topology[hostID] === "undefined") {
  let node = {
    os: "NOS 2.8.7",
    software: "NJEF",
    lid: `M${mid}`
  };
  if (defaultRoute !== hostID) node.link = defaultRoute;
  topology[hostID] = node;
}

//
// Generate an NDL modset. Define a terminal for the local system's default
// route and for each node explicitly linked to the local system.
//
if (npuNode < 10) npuNode = `0${npuNode}`;
let terminalDefns = [];
let userDefns     = [];
let lineNumber    = 1;
let routingNode   = null;
let adjacentNodes = [];

const appendTerminalDefn = node => {
  terminalDefns = terminalDefns.concat([
    `*  ${node.id}`,
    `LI${npuNode}P${node.claPort}: LINE      PORT=${node.claPort},LTYPE=S2,TIPTYPE=TT13.`,
    `         ${node.terminalName}:  TERMDEV,STIP=USER,TC=TC29,DT=CON,ABL=7,`,
    "                           DBZ=1020,UBZ=11,DBL=7,UBL=12,XBZ=400,",
    `                           HN=${couplerNode},AUTOCON.`
  ]);
};

if (typeof topology[hostID].link !== "undefined") {
  routingNode = topology[topology[hostID].link];
  adjacentNodes.push(routingNode);
  const portNum = `${nextPort.toString(16)}`;
  routingNode.id           = topology[hostID].link;
  routingNode.terminalName = `TE${npuNode}P${portNum}`;
  routingNode.claPort      = portNum;
  routingNode.lineNumber   = lineNumber++;
  appendTerminalDefn(routingNode);
  userDefns.push(`${routingNode.terminalName}: USER,     NJFUSER.`);
  nextPort  += 1;
  portCount -= 1;
  topology[hostID].lineNumber = lineNumber++;
}
for (const key of Object.keys(topology)) {
  let node = topology[key];
  if (node.link === hostID && portCount > 0) {
    adjacentNodes.push(node);
    const portNum = `${nextPort.toString(16)}`;
    node.id           = key;
    node.terminalName = `TE${npuNode}P${portNum}`;
    node.claPort      = portNum;
    node.lineNumber   = lineNumber++;
    appendTerminalDefn(node);
    userDefns.push(`${node.terminalName}: USER,     NJFUSER.`);
    nextPort  += 1;
    portCount -= 1;
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

const calculateRoute = (node) => {
  if (typeof node.route !== "undefined") return node.route;
  for (const adjacentNode of adjacentNodes) {
    if (adjacentNode.id === node.link) {
      node.route = adjacentNode.id;
      return adjacentNode.id;
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
};

//  1. Generate local system's node definition
hcfSource = hcfSource.concat([
  "*",
  "*  LOCAL NJE NODE",
  "*",
  `NODE,${nodeNumber},${hostID},M${mid},,CDC,NET,JOB.`,
  `OWNNODE,${nodeNumber},CYBER,NETOPS.`
]);
nodeNumber += 1;

//  2. Generate definitions of adjacent nodes
if (adjacentNodes.length > 0) {
  hcfSource = hcfSource.concat([
    "*",
    "*  ADJACENT NJE NODES",
    "*"
  ]);
  for (const node of adjacentNodes) {
    node.nodeNumber = nodeNumber++;
    hcfSource.push(
      `NODE,${node.nodeNumber},${node.id},${node.lid},,${node.software === "NJEF" ? "CDC" : "IBM "},NET,JOB.`
    );
  }

//  3. Generate line and auto-start line definitions
  hcfSource = hcfSource.concat([
    "*",
    "*  NETWORK LINE DEFINITIONS",
    "*"
  ]);
  for (const node of adjacentNodes) {
    hcfSource.push(
      `LNE,${node.lineNumber},${node.terminalName},1000.`
    );
  }
  hcfSource = hcfSource.concat([
    "*",
    "*  AUTO-START DIRECTIVES",
    "*"
  ]);
  for (const node of adjacentNodes) {
    hcfSource.push(
      `ASLNE,${node.lineNumber},ST1,SR1${node.software === "NJEF" ? ",JT1,JR1" : ""}.`
    );
  }
}

//  4. Generate FAMILY sattements for routing to non-adjacent nodes.
//     Associate each non-adjacent node with the closes adjacent node
//     through which the non-adjacent node may be reached.
let nonadjacentNodes = [];
for (const key of Object.keys(topology)) {
  let node = topology[key];
  if (key !== hostID && node.link !== hostID) {
    node.id = key;
    const route = calculateRoute(node);
    if (route !== null) nonadjacentNodes.push(node);
  }
}
if (nonadjacentNodes.length > 0) {
  hcfSource = hcfSource.concat([
    "*",
    "*  ROUTES TO NON-ADJACENT NODES",
    "*"
  ]);
  let n = 1;
  for (const node of nonadjacentNodes) {
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
  `npuNode=${parseInt(npuNode, 10)}`
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
let localNode = topology[hostID];
let defaultLocalIP = null;
if (typeof localNode.publicAddress !== "undefined") {
  let parts = localNode.publicAddress.split(":");
  if (parts[0] !== "0.0.0.0") {
    defaultLocalIP = parts[0];
  }
}
for (const node of adjacentNodes) {
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
  let termDefn = `terminals=${listenPort},0x${node.claPort.toString(16)},1,nje`
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
.then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job"))
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
  let lines = output.split("\n").slice(1);
  for (const line of lines) {
    if (line.startsWith("NPID,")) {
      let match = line.match(/NPID,PID=([^,]*),MFTYPE=([^,.]*)/);
      if (match !== null) {
        currentPid = match[1];
        lidConf[currentPid] = {
          MFTYPE: match[2],
          lids: {}
        };
      }
    }
    else if (line.startsWith("NLID,") && currentPid !== null) {
      let match = line.match(/NLID,LID=([^,]*)/);
      if (match !== null) {
        lidConf[currentPid].lids[match[1]] = line;
      }
    }
  }
  //
  // Create/update PID and LID definitions from the adjacent nodes list
  //
  let localPid = {
    MFTYPE: "NOS2",
    lids: {}
  };
  localPid.lids[`M${mid}`] = `NLID,LID=M${mid}.`;
  lidConf[`M${mid}`] = localPid;
  for (const node of adjacentNodes) {
    if (typeof node.lid !== "undefined") {
      localPid.lids[node.lid] = `NLID,LID=${node.lid},AT=STOREF.`;
      let mfType = "UNKNOWN";
      if (node.software === "NJEF")
        mfType = "NOS2";
      else if (node.software === "RSCS")
        mfType = "CMS";
      else if (node.software === "NJE38")
        mfType = "MVS";
      else if (node.software === "JNET")
        mfType = "VMS";
      let remotePid = {
        MFTYPE: mfType,
        lids: {}
      };
      remotePid.lids[node.lid] = `NLID,LID=${node.lid}.`;
      lidConf[node.lid] = remotePid;
    }
  }
  //
  // Create new/updated LIDCMid file
  //
  let lidText = [
    `LIDCM${mid}`
  ];
  const pids = Object.keys(lidConf).sort();
  for (const pid of pids) {
    let pidDefn = lidConf[pid];
    lidText.push(`NPID,PID=${pid},MFTYPE=${pidDefn.MFTYPE}.`);
    let lids = Object.keys(pidDefn.lids).sort();
    for (const lid of lids) {
      lidText.push(pidDefn.lids[lid]);
    }
  }
  lidText.push("");

  return replaceFile(`LIDCM${mid}`, lidText.join("\n"));
})
.then(() => dtc.say(`Move LIDCM${mid} to SYSTEMX ...`))
.then(() => dtc.dis([
  `GET,LIDCM${mid}.`,
  `PURGE,LIDCM${mid}.`,
  "SUI,377777.",
  `REPLACE,LIDCM${mid}.`,
  `PERMIT,LIDCM${mid},INSTALL=W.`
], "MOVELID", 1))
.then(() => dtc.say("NJF configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
