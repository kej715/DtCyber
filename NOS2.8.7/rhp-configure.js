#!/usr/bin/env node
//
// Create the RHP configuration including:
//
//  1. NDL modset defining trunks and INCALL/OUTCALL definitions
//     used by RHP
//  2. PID/LID definitions for RHP nodes
//  3. Update of cyber.ovl file to include trunk definitions
//     for links to RHP nodes
//
// After creating the configuration files:
//  1. The NDL modset is applied and the NDL configuration
//     file is compiled
//  2. The LIDCMid file is updated to add PIDs/LIDs of
//     RHP nodes
//

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const mid         = utilities.getMachineId(dtc);
const hostID      = utilities.getHostId(dtc);
const npuNode     = utilities.getNpuNode(dtc);
const rhpTopology = utilities.getRhpTopology(dtc);

const localNode   = rhpTopology[hostID];

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

//
// Determine current machine ID, coupler node, and npu node from cyber.ini and cyber.ovl files.
//
const iniProps = utilities.getIniProperties(dtc);
let currentCouplerNode = 1;
let currentMid         = "01";
let currentNpuNode     = 2;
if (typeof iniProps["sysinfo"] !== "undefined") {
  for (const line of iniProps["sysinfo"]) {
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim().toUpperCase();
    let value = line.substring(ei + 1).trim().toUpperCase();
    if (key === "MID") {
      currentMid = value;
    }
  }
}
if (typeof iniProps["npu.nos287"] !== "undefined") {
  for (const line of iniProps["npu.nos287"]) {
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim().toUpperCase();
    let value = line.substring(ei + 1).trim();
    if (key === "COUPLERNODE") {
      currentCouplerNode = parseInt(value);
    }
    else if (key === "NPUNODE") {
      currentNpuNode = parseInt(value);
    }
  }
}

const isCouplerNodeChange = currentCouplerNode  !== couplerNode;
const isMidChange         = currentMid          !== mid;
const isNpuNodeChange     = currentNpuNode      !== npuNode;
const isNodeChange        = isCouplerNodeChange ||  isNpuNodeChange;

//
// Generate an NDL modset if the machine identifier, coupler node, or NPU node are not default
// values, or the RHP topology has at least one other node linked to the local node.
//
let edits        = 0;

let ndlModset = [
  "RHP",
  "*IDENT RHP",
  "*DECK NETCFG"
];
const nodeNames = Object.keys(rhpTopology).sort((n1, n2) => {
  return rhpTopology[n1].couplerNode - rhpTopology[n2].couplerNode;
});
const linkNames = Object.keys(localNode.links).sort((n1, n2) => {
  return rhpTopology[n1].couplerNode - rhpTopology[n2].couplerNode;
});

//
// Generate trunk definitions if at least one other node is linked to the local node.
//
if (linkNames.length > 0) {
  edits += 1;
  ndlModset.push("*D 50,51");
  let trunks = [];
  for (const nodeName of nodeNames) {
    let node = rhpTopology[nodeName];
    for (const linkName of Object.keys(node.links)) {
      let peer = rhpTopology[linkName];
      let trunkName1 = `TR${toHex(node.npuNode)}X${toHex(peer.npuNode)}`;
      let trunkName2 = `TR${toHex(peer.npuNode)}X${toHex(node.npuNode)}`;
      if (trunks.indexOf(trunkName1) === -1 && trunks.indexOf(trunkName2) === -1) {
        trunks.push(trunkName1, trunkName2);
        ndlModset.push(`${trunkName1}: TRUNK     N1=NP${toHex(node.npuNode)},P1=${toHex(node.links[linkName])},NOLOAD1,`
       +                                        `N2=NP${toHex(peer.npuNode)},P2=${toHex(peer.links[nodeName])},NOLOAD2.`);
      }
    }
  }
}

//
// Modify the local NPU definition if the couple node, NPU node, or machine ID are changing.
//
if (isNodeChange || isMidChange) {
  edits += 1;
  ndlModset = ndlModset.concat([
    "*D 55,59",
    `*                   DEFINITIONS FOR NP${toHex(npuNode)}                       *`,
    "*                                                              *",
    `*   NP${toHex(npuNode)} IS ATTACHED TO MAINFRAME M${mid} VIA COUPLER CP${toHex(couplerNode)}M${mid}.     *`,
    `*   COUPLER CP${toHex(couplerNode)}M${mid} IS THE SOURCE NODE FOR A LOGICAL LINK TO   *`,
    `*   NPU NP${toHex(npuNode)},                                                  *`,
    "*D 63,67",
    `NP${toHex(npuNode)}:    NPU,      NODE=${npuNode},VARIANT=SM1,DMP=NO.`,
    `         SUPLINK,  LLNAME=LL${toHex(couplerNode)}N${toHex(npuNode)}.`,
    "",
    `CP${toHex(couplerNode)}M${mid}: COUPLER,  NODE=${couplerNode}.`,
    `         LL${toHex(couplerNode)}N${toHex(npuNode)}:  LOGLINK, NCNAME=NP${toHex(npuNode)}.`
  ]);
}

//
// Generate logical link definitions for the local NPU if at least one other node is linked to the local node.
//
let logicalLinks = [];
if (linkNames.length > 0) {
  edits += 1;
  ndlModset.push("*D 68,70");
  let node = localNode;
  for (const linkName of Object.keys(node.links)) {
    let peer = rhpTopology[linkName];
    let linkName1 = `LL${toHex(node.couplerNode)}C${toHex(peer.couplerNode)}`;
    let linkName2 = `LL${toHex(peer.couplerNode)}C${toHex(node.couplerNode)}`;
    if (logicalLinks.indexOf(linkName1) === -1 && logicalLinks.indexOf(linkName2) === -1) {
      logicalLinks.push(linkName1, linkName2);
      ndlModset.push(`         ${linkName1}:  LOGLINK, NCNAME=CP${toHex(peer.couplerNode)}${peer.lid}.`);
    }
  }
}

//
// Modify terminal definitions on the local NPU if the coupler node or NPU node are changing.
//
if (isNodeChange) {
  edits += 1;
  ndlModset = ndlModset.concat([
    "*D 74",
    `*              ASYNCHRONOUS TERMINALS ON NP${toHex(npuNode)}                  *`,
    "*D 78,79",
    `LI${toHex(npuNode)}P03: LINE,     PORT=03,L9600.`,
    `         PR${toHex(npuNode)}P03:  TERMDEV,TC=X364,AUTOCON,HN=${couplerNode},LK=YES,OC=YES,`,
    "*D 82,83",
    `LI${toHex(npuNode)}P:   GROUP,    PORT=04,L9600,NI=16.`,
    `         TE${toHex(npuNode)}P:    TERMDEV, TASX364.`,
    "*D 90",
    `*           HASP TERMINALS ON NP${toHex(npuNode)}                             *`,
    "*D 94,106",
    `LI${toHex(npuNode)}P24: LINE      PORT=24,LTYPE=S2,TIPTYPE=HASP.`,
    `         TERMINAL  TC=HPRE,RIC=YES.`,
    `         CO${toHex(npuNode)}P24:  DEVICE,DT=CON,HN=${couplerNode},PW=0,AUTOCON.`,
    `         CR${toHex(npuNode)}P24:  DEVICE,DT=CR,STREAM=1.`,
    `         LP${toHex(npuNode)}P24:  DEVICE,DT=LP,STREAM=1,SDT=A9,PW=132.`,
    `         PU${toHex(npuNode)}P24:  DEVICE,DT=CP,STREAM=1.`,
    "",
    `LI${toHex(npuNode)}P25: LINE      PORT=25,LTYPE=S2,TIPTYPE=HASP.`,
    `         TERMINAL  TC=HPRE,RIC=YES.`,
    `         CO${toHex(npuNode)}P25:  DEVICE,DT=CON,HN=${couplerNode},PW=0,AUTOCON.`,
    `         CR${toHex(npuNode)}P25:  DEVICE,DT=CR,STREAM=1.`,
    `         LP${toHex(npuNode)}P25:  DEVICE,DT=LP,STREAM=1,SDT=A9,PW=132.`,
    `         PU${toHex(npuNode)}P25:  DEVICE,DT=CP,STREAM=1.`,
    "*D 110",
    `*           REVERSE HASP TERMINALS ON NP${toHex(npuNode)}                     *`,
    "*D 119",
    `*           NJE TERMINALS ON NP${toHex(npuNode)}                              *`
  ]);
}

//
// Generate NPU, coupler, and logical link definitions for remote NPU's if at least one other node is linked to the local node.
//
if (linkNames.length > 0) {
  edits += 1;
  ndlModset.push("*D 125,133");
  for (const nodeName of linkNames) {
    let node = rhpTopology[nodeName];
    ndlModset = ndlModset.concat([
      "",
      "****************************************************************",
      "*                                                              *",
      `*                   DEFINITIONS FOR NP${toHex(node.npuNode)}                       *`,
      "*                                                              *",
      `*   NP${toHex(node.npuNode)} IS ATTACHED TO MAINFRAME ${node.lid} VIA COUPLER CP${toHex(node.couplerNode)}${node.lid}.     *`,
      `*   COUPLER CP${toHex(node.couplerNode)}${node.lid} IS THE SOURCE NODE FOR A LOGICAL LINK TO   *`,
      `*   NPU NP${toHex(node.npuNode)},                                                  *`,
      "*                                                              *",
      "****************************************************************",
      `NP${toHex(node.npuNode)}:    NPU,      NODE=${node.npuNode},VARIANT=SM1,DMP=NO.`,
      `         SUPLINK,  LLNAME=LL${toHex(node.couplerNode)}N${toHex(node.npuNode)}.`,
      "",
      `CP${toHex(node.couplerNode)}${node.lid}: COUPLER,  NODE=${node.couplerNode}.`,
      `         LL${toHex(node.couplerNode)}N${toHex(node.npuNode)}:  LOGLINK, NCNAME=NP${toHex(node.npuNode)}.`
    ]);
    for (const linkName of Object.keys(node.links)) {
      let peer = rhpTopology[linkName];
      let linkName1 = `LL${toHex(node.couplerNode)}C${toHex(peer.couplerNode)}`;
      let linkName2 = `LL${toHex(peer.couplerNode)}C${toHex(node.couplerNode)}`;
      if (logicalLinks.indexOf(linkName1) === -1 && logicalLinks.indexOf(linkName2) === -1) {
        logicalLinks.push(linkName1, linkName2);
        ndlModset.push(`         ${linkName1}:  LOGLINK, NCNAME=CP${toHex(peer.couplerNode)}${peer.lid}.`);
      }
    }
  }
}

//
// Modify the LCFFILE declaration and TCP/IP Gateway OUTCALL definition if the coupler node, NPU node,
// or machine ID are changing.
//
if (isNodeChange || isMidChange) {
  edits += 1;
  ndlModset = ndlModset.concat([
    "*D 139",
    `*            LOCAL CONFIGURATION FILE FOR HOST M${mid}             *`,
    "*D 143,144",
    `LCFM${mid} : LFILE.`,
    `         TITLE,    LOCAL CONFIGURATION FOR HOST M${mid}.`,
    "*D 181,182",
    `OUTCALL,NAME1=TCPIPGW,NAME2=H${mid},SNODE=${couplerNode},DNODE=255,NETOSD=DDV,`,
    `        ABL=7,DBL=7,DBZ=2000,UBL=7,UBZ=18,SERVICE=GW_TCPIP_M${mid}.`
  ]);
}

//
// Generate INCALL and OUTCALL definitions for QTFS and PTFS if at least one other node is linked to the local node.
//
if (linkNames.length > 1) {
  edits += 1;
  ndlModset = ndlModset.concat([
    "*D 184,185",
    "INCALL,FAM=0,UNAME=NETOPS,ANAME=QTFS,DBL=7,ABL=7,DBZ=1024."
  ]);
  let node = rhpTopology[hostID];
  for (const linkName of Object.keys(localNode.links)) {
    let peer = rhpTopology[linkName];
    ndlModset.push(`OUTCALL,NAME1=QTFS,PID=${peer.lid},SNODE=${couplerNode},DNODE=${peer.couplerNode},DBL=7,ABL=7,DBZ=1024.`);
  }
  ndlModset = ndlModset.concat([
    "*D 187,188",
    "INCALL,FAM=0,UNAME=NETOPS,ANAME=PTFS,DBL=7,ABL=7,DBZ=1024."
  ]);
  for (const linkName of Object.keys(localNode.links)) {
    let peer = rhpTopology[linkName];
    ndlModset.push(`OUTCALL,NAME1=PTFS,PID=${peer.lid},SNODE=${couplerNode},DNODE=${peer.couplerNode},DBL=7,ABL=7,DBZ=1024.`);
  }
}

//
// Modify USER definitions if the NPU node is changing.
//
if (isNpuNodeChange) {
  edits += 1;
  ndlModset = ndlModset.concat([
    "*D 201",
    `PR${toHex(npuNode)}P03: USER,     MFAM=CYBER,MUSER=PRINT05,MAPPL=PSU.`
  ]);
  ndlModset.push("*D 203,218");
  for (let port = 0x04; port <= 0x13; port++) {
    ndlModset.push(`TE${toHex(npuNode)}P${toHex(port)}: USER,     IAFUSER.`);
  }
  ndlModset.push("*D 223,224");
  for (let port = 0x24; port <= 0x25; port++) {
    ndlModset.push(`CO${toHex(npuNode)}P${toHex(port)}: USER,     RBFUSER,MUSER=RJE${port - 0x23}.`);
  }
}

ndlModset.push("");

//
// If an overlay file exists, read it, and merge its definitions with
// definitions generated from customization properties and RHP node
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
    if (key !== "TERMINALS" || value.toUpperCase().indexOf(",TRUNK,") < 0) {
      ovlText.push(line);
    }
  }
}
let ci = localNode.addr.indexOf(":");
let localPort = parseInt(ci === -1 ? localNode.addr : localNode.addr.substring(ci + 1));
for (const nodeName of Object.keys(localNode.links)) {
  let node = rhpTopology[nodeName];
  ovlText.push(`terminals=${localPort},0x${toHex(localNode.links[nodeName])},1,trunk,${node.addr},${node.name},${node.couplerNode}`);
}
ovlProps["npu.nos287"] = ovlText;

ovlProps["sysinfo"] = [
  `MID=${mid}`
];

let ovlLines = [];
for (const key of Object.keys(ovlProps)) {
  ovlLines.push(`[${key}]`);
  for (const line of ovlProps[key]) {
    ovlLines.push(`${line}`);
  }
}
ovlLines.push("");

if (edits > 0) {
  fs.writeFileSync("RHPNDL.mod", ndlModset.join("\n"));
  dtc.connect()
  .then(() => dtc.expect([ {re:/Operator> $/} ]))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.say("Update NDL with RHP topology definitions ..."))
  .then(() => {
      const job = [
        "$GET,NDLMODS/NA.",
        "$IF,FILE(NDLMODS,AS),EDIT.",
        "$  COPY,INPUT,MOD.",
        "$  REWIND,MOD.",
        "$  LIBEDIT,P=NDLMODS,B=MOD,I=0,C.",
        "$ELSE,EDIT.",
        "$  COPY,INPUT,NDLMODS.",
        "$ENDIF,EDIT.",
        "$REPLACE,NDLMODS."
      ];
      const options = {
        jobname:  "RHPMOD",
        username: "NETADMN",
        password: "NETADMN",
        data:     `${ndlModset.join("\n")}\n`
      };
      return dtc.createJobWithOutput(12, 4, job, options);
  })
  .then(() => dtc.say("Compile updated NDL ..."))
  .then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job"))
  .then(() => dtc.exec("node", ["lid-configure"]))
  .then(() => {
    fs.writeFileSync("cyber.ovl", ovlLines.join("\n"));
    return Promise.resolve();
  });
  .then(() => dtc.say("RHP configuration completed successfully"))
  .then(() => {
    process.exit(0);
  })
  .catch(err => {
    console.log(err);
    process.exit(1);
  });
}
else {
  fs.writeFileSync("cyber.ovl", ovlLines.join("\n"));
  console.log(`${new Date().toLocaleTimeString()} No NDL modifications needed`);
}
