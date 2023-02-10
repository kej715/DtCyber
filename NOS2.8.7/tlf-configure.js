#!/usr/bin/env node
//
// Create the TLF configuration including:
//
//  1. NDL modset defining the terminals used by TLF
//  2. The source for the TLF configuration file
//  3. PID/LID definitions for TLF nodes
//  4. Update of cyber.ovl file to include terminal
//     definitions for DtCyber
//
// After creating the configuration files:
//  1. The NDL modset is applied and the NDL configuration
//     file is compiled
//  2. The host configuration file is compiled, and the
//     resulting TCFFILE is made available to SYSTEMX.
//  3. The LIDCMid file is updated to add PIDs/LIDs of
//     nodes with LIDs
//

const fs      = require("fs");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

const spoolerTypes = {
  JES2: {
    mfType: "MVS",
    skipCount: 2
  },
  NOS: {
    mfType: "NOS2",
    skipCount: 2
  },
  PRIME: {
    mfType: "PRIME",
    skipCount: 0
  },
  RSCS: {
    mfType: "VM/CMS",
    skipCount: 2
  }
};

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
// Update network parameters to reflect customizations, if any.
//
let nextPort      = 0x28;
let nodes         = [];
let portCount     = 8;

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
    else if (key === "TLFNODE") {
      //
      //  tlfNode=<name>,<lid>,<spooler>,<addr>
      //    [,R<remote-id>][,P<password>][,B<block-size>]
      let items = value.split(",");
      if (items.length >= 4) {
        let node = {
          name: items.shift(),
          lid: items.shift(),
          spooler: items.shift(),
          addr: items.shift(),
          remoteId: "",
          password: "",
          blockSize: 400
        };
        if (typeof spoolerTypes[node.spooler] === "undefined") {
          throw new Error(`Unrecognized TLF spooler type: ${node.spooler}`);
        }
        const spoolerInfo = spoolerTypes[node.spooler];
        for (const key of Object.keys(spoolerInfo)) node[key] = spoolerInfo[key];
        while (items.length > 0) {
          let item = items.shift();
          if (item.startsWith("B")) {
            node.blockSize = parseInt(item.substring(1));
          }
          else if (item.startsWith("P")) {
            node.password = parseInt(item.substring(1));
          }
          else if (item.startsWith("R")) {
            node.remoteId = parseInt(item.substring(1));
          }
        }
        nodes.push(node);
      }
    }
    else if (key === "TLFPORTS") {
      let items = value.split(",");
      if (items.length > 0) nextPort  = parseInt(items.shift());
      if (items.length > 0) portCount = parseInt(items.shift());
    }
  }
}

//
// Generate an NDL modset. Define a terminal for the local system's default
// route and for each node explicitly linked to the local system.
//
if (npuNode < 10) npuNode = `0${npuNode}`;
let terminalDefns = [];
let userDefns     = [];

const appendTerminalDefn = node => {
  terminalDefns = terminalDefns.concat([
    `*  ${node.name}`,
    `LI${npuNode}P${node.claPort}: LINE      PORT=${node.claPort},LTYPE=S2,TIPTYPE=TT12.`,
    "         TERMINAL  TC=TC28,STIP=USER.",
    `         ${node.console}:  DEVICE,REVHASP,HN=${couplerNode},DT=CON,ABL=2,DBZ=120,PW=120.`,
    `         CR${npuNode}P${node.claPort}:  DEVICE,REVHASP,HN=${couplerNode},DT=CR,DO=1,TA=1,ABL=3,DBZ=810,`,
    "                 PW=137.",
    `         LP${npuNode}P${node.claPort}:  DEVICE,REVHASP,HN=${couplerNode},DT=LP,DO=1,TA=1,ABL=0,DBZ=0.`,
    `         PL${npuNode}P${node.claPort}:  DEVICE,REVHASP,HN=${couplerNode},DT=PL,DO=1,TA=1,ABL=0,DBZ=0.`
  ]);
};
for (const node of nodes) {
  if (portCount < 1) throw new Error("Insufficient number of TLF ports defined");
  const portNum      = `${nextPort.toString(16)}`;
  node.claPort       = portNum;
  node.console       = `CO${npuNode}P${node.claPort}`;
  appendTerminalDefn(node);
  userDefns.push(`${node.console}: USER,     TLFUSER.`);
  nextPort  += 1;
  portCount -= 1;
}

const ndlModset = [
  "TLF",
  "*IDENT TLF",
  "*DECK NETCFG",
  "*I 42",
  " ",
  "REVHASP: DEFINE,   AUTOCON, DBL=7, UBL=7, UBZ=100, XBZ=400.",
  "*D 114,115"]
  .concat(terminalDefns)
  .concat([
  "*D 226,227"
  ])
  .concat(userDefns)
  .concat("");

fs.writeFileSync("mods/TLF/NDL.mod", ndlModset.join("\n"));

//
// Generate the source for the TLF configuration file.
//
let tcfSource     = [
  `TCFM${mid}.`
];

// Generate LIDS section
tcfSource = tcfSource.concat([
  "LIDS."
]);
for (const node of nodes) {
  tcfSource = tcfSource.concat([
    `${node.lid}=${node.spooler},${node.skipCount},HFRI,HFJC.`
  ]);
}

// Generate LINES section
tcfSource = tcfSource.concat([
  "LINES."
]);
for (const node of nodes) {
  tcfSource = tcfSource.concat([
    `${node.console}=${node.spooler},${node.remoteId},${node.password},,,,${node.lid}.`
  ]);
}

tcfSource = tcfSource.concat([
  "END."
]);

//
// If an overlay file exists, read it, and merge its definitions with
// definitions generated from customization properties and TLF node
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
    if (key !== "TERMINALS" || value.toUpperCase().indexOf(",RHASP,") < 0) {
      ovlText.push(line);
    }
  }
}
for (const node of nodes) {
  let termDefn = `terminals=0,0x${node.claPort.toString(16)},1,rhasp,${node.addr}`;
  if (typeof node.blockSize !== "undefined") termDefn += `,B${node.blockSize}`;
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

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Update NDL with TLF terminal definitions ..."))
.then(() => dtc.runJob(12, 4, "opt/tlf-ndl.job"))
.then(() => dtc.say("Compile updated NDL ..."))
.then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job"))
.then(() => {
  if (nodes.length > 0) {
    return dtc.say("Create/update TLF configuration file ...")
    .then(() => {
      const options = {
        username: "NETADMN",
        password: "NETADMN"
      };
      return replaceFile("TCFSRC", tcfSource.join("\n"), options);
    })
    .then(() => dtc.say("Compile updated TLF configuration file ..."))
    .then(() => dtc.runJob(12, 4, "opt/tlf-compile-tcf.job"))
    .then(() => dtc.say("Move TCFFILE to NETADMN ..."))
    .then(() => dtc.dis([
      "GET,TCFFILE.",
      "PURGE,TCFFILE.",
      "SUI,25.",
      "PURGE,TCFFILE/NA.",
      "SAVE,TCFFILE/CT=PU,AC=N,M=R."
    ], "MOVETCF", 1))
    .then(() => dtc.disconnect())
    .then(() => dtc.exec("node", ["lid-configure"]));
  }
  else {
    return dtc.say("Purge TLF configuration file ...")
    .then(() => dtc.dis("PURGE,TCFFILE/NA.", 25));
  }
})
.then(() => dtc.say("TLF configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
