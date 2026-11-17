#!/usr/bin/env node
//
// Update the NDL to reflect the NHP configuration and RHP topology
//

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const mid      = utilities.getMachineId(dtc);
const hostID   = utilities.getHostId(dtc);
const config   = utilities.getNhpConfiguration(dtc);
const topology = utilities.getRhpTopology(dtc);

let configKeys = [];
for (const key of Object.keys(config)) {
  if (key.startsWith("MDI") || key.startsWith("NPU")) configKeys.push(key);
}
configKeys = configKeys.sort((k1, k2) => {
  return config[k1].couplerNode - config[k2].couplerNode;
});

const toHex = value => {
  return (value < 16 ? `0${value.toString(16)}` : value.toString(16)).toUpperCase();
};

let ndlModset = [
  "SITEMOD",
  "*IDENT SITEMOD",
  "*DECK NETCFG",
  "*I 42",
  "",
  "REVHASP: DEFINE,   AUTOCON, DBL=7, UBL=7, UBZ=100, XBZ=400."
];

let trunkNames   = [];
let trunkDefns   = [];
let logicalLinks = [];
let remoteNpus   = {};
let lidMap       = {};
let couplerMap   = {};
let trunkPortMap = {};
for (const node of topology) {
  lidMap[node.id] = node.lid;
  couplerMap[`${node.id}.${node.npuNode}`] = node.couplerNode;
  for (const linkName of Object.keys(node.links)) {
    let link = node.links[linkName];
    trunkPortMap[`TR${toHex(node.npuNode)}X${toHex(link.npuNode)}`] = link.localClaPort;
  }
}

//
// Generate trunk definitions if at least one other node is linked to the local node.
//
for (const node of topology) {
  for (const linkName of Object.keys(node.links)) {
    let link = node.links[linkName];
    let trunkName1 = `TR${toHex(node.npuNode)}X${toHex(link.npuNode)}`;
    let trunkName2 = `TR${toHex(link.npuNode)}X${toHex(node.npuNode)}`;
    if (trunkNames.indexOf(trunkName1) === -1 && trunkNames.indexOf(trunkName2) === -1) {
      trunkNames.push(trunkName1, trunkName2);
      if (typeof trunkPortMap[trunkName2] === "undefined") {
        throw new Error(`Invalid NPU node number ${link.npuNode} in rhpNode definition`);
      }
      trunkDefns.push(`${trunkName1}: TRUNK     N1=NP${toHex(node.npuNode)},P1=${toHex(link.localClaPort)},NOLOAD1,`
     +                                         `N2=NP${toHex(link.npuNode)},P2=${toHex(trunkPortMap[trunkName2])},NOLOAD2.`);
    }
  }
  if (node.id === hostID) {
    for (const linkName of Object.keys(node.links)) {
      let link = node.links[linkName];
      let id   = `${link.id}.${link.npuNode}`;
      let remoteNpu = {
        id: id,
        hostId: link.id,
        lid: lidMap[link.id],
        npuNode: link.npuNode,
        couplerNode: couplerMap[id]
      };
      remoteNpus[id] = remoteNpu;
    }
  }
}

if (trunkDefns.length > 0) {
  ndlModset = ndlModset.concat(
    "*D 50,51",
    trunkDefns
  );
}
else if (config.isDefault) {
  process.exit(0);
}

ndlModset.push("*D 52,133");

//
//  Generate NDL for each of the MDIs and NPUs connected directly to the local host.
//
for (const key of configKeys) {
  if (key.startsWith("MDI") || key.startsWith("NPU")) {
    let netDevice = config[key];
    ndlModset = ndlModset.concat(
    "",
    "****************************************************************",
    "*                                                              *",
    `*                   DEFINITIONS FOR NP${toHex(netDevice.npuNode)}                       *`,
    "*                                                              *",
    `*   NP${toHex(netDevice.npuNode)} IS ATTACHED TO MAINFRAME M${mid} VIA COUPLER CP${toHex(netDevice.couplerNode)}M${mid}.     *`,
    `*   COUPLER CP${toHex(netDevice.couplerNode)}M${mid} IS THE SOURCE NODE FOR A LOGICAL LINK TO   *`,
    `*   NPU NP${toHex(netDevice.npuNode)}.                                                  *`,
    "*                                                              *",
    "****************************************************************",
    "",
    `NP${toHex(netDevice.npuNode)}:    NPU,      NODE=${netDevice.npuNode},VARIANT=SM1,DMP=NO.`,
    `         SUPLINK,  LLNAME=LL${toHex(netDevice.couplerNode)}N${toHex(netDevice.npuNode)}.`,
    "",
    `CP${toHex(netDevice.couplerNode)}M${mid}: COUPLER,  NODE=${netDevice.couplerNode}.`,
    `         LL${toHex(netDevice.couplerNode)}N${toHex(netDevice.npuNode)}:  LOGLINK, NCNAME=NP${toHex(netDevice.npuNode)}.`
    );
    for (const node of topology) {
      if (node.id === hostID && node.npuNode === netDevice.npuNode && node.couplerNode === netDevice.couplerNode) {
        for (const linkName of Object.keys(node.links)) {
          let link = node.links[linkName];
          let couplerNode = couplerMap[`${link.id}.${link.npuNode}`];
          let linkName1 = `LL${toHex(node.couplerNode)}C${toHex(couplerNode)}`;
          let linkName2 = `LL${toHex(couplerNode)}C${toHex(node.couplerNode)}`;
          if (logicalLinks.indexOf(linkName1) === -1 && logicalLinks.indexOf(linkName2) === -1) {
            logicalLinks.push(linkName1, linkName2);
            ndlModset.push(`         ${linkName1}:  LOGLINK, NCNAME=CP${toHex(couplerNode)}${lidMap[link.id]}.`);
          }
        }
      }
    }
    //
    //  Create definitions for printer terminals, if any
    //
    if (typeof netDevice.raw !== "undefined") {
      ndlModset = ndlModset.concat(
      "",
      "****************************************************************",
      "*                                                              *",
      `*              ASYNCHRONOUS PRINTERS ON NP${toHex(netDevice.npuNode)}                   *`,
      "*                                                              *",
      "****************************************************************",
      ""
      );
      for (const termDef of netDevice.raw) {
        if (termDef.claPortCount > 1) {
          ndlModset = ndlModset.concat(
          `LI${toHex(netDevice.npuNode)}P:   GROUP,    PORT=${toHex(termDef.firstClaPort)},L9600,NI=${termDef.claPortCount}.`,
          `         PR${toHex(netDevice.npuNode)}P:    TERMDEV,TASX364,AUTOCON,HN=${toHex(netDevice.couplerNode)},LK=YES,OC=YES,`,
          "                   EP=NO,PA=N,PW=0,PL=0,ABL=3,DBL=2."
          );
        }
        else {
          ndlModset = ndlModset.concat(
          `LI${toHex(netDevice.npuNode)}P${toHex(termDef.firstClaPort)}: LINE,     PORT=${toHex(termDef.firstClaPort)},L9600.`,
          `         PR${toHex(netDevice.npuNode)}P${toHex(termDef.firstClaPort)}:  TERMDEV,TC=X364,AUTOCON,HN=${netDevice.couplerNode},LK=YES,OC=YES,`,
          "                   EP=NO,PA=N,PW=0,PL=0,ABL=3,DBL=2."
          );
        }
      }
    }
    //
    //  Create definitions for async terminals, if any
    //
    if (typeof netDevice.telnet !== "undefined") {
      ndlModset = ndlModset.concat(
      "",
      "****************************************************************",
      "*                                                              *",
      `*              ASYNCHRONOUS TERMINALS ON NP${toHex(netDevice.npuNode)}                  *`,
      "*                                                              *",
      "****************************************************************",
      ""
      );
      for (const termDef of netDevice.telnet) {
        ndlModset = ndlModset.concat(
        `LI${toHex(netDevice.npuNode)}P:   GROUP,    PORT=${toHex(termDef.firstClaPort)},L9600,NI=${termDef.claPortCount}.`,
        `         TE${toHex(netDevice.npuNode)}P:    TERMDEV, TASX364,HN=${toHex(netDevice.couplerNode)}.`
        );
      }
    }
    //
    //  Create definitions for pterm terminals, if any
    //
    if (utilities.isInstalled("cybis-base") && typeof netDevice.pterm !== "undefined") {
      ndlModset = ndlModset.concat(
      "",
      "****************************************************************",
      "*                                                              *",
      `*              CYBIS TERMINALS ON NP${toHex(netDevice.npuNode)}                         *`,
      "*                                                              *",
      "****************************************************************",
      ""
      );
      for (const termDef of netDevice.pterm) {
        ndlModset = ndlModset.concat(
        `LI${toHex(netDevice.npuNode)}P:   GROUP,    PORT=${toHex(termDef.firstClaPort)},L9600,NI=${termDef.claPortCount}.`,
        `         TE${toHex(netDevice.npuNode)}P:    TERMDEV, TASX364,AUTOCON,HN=${toHex(netDevice.couplerNode)},ABL=7.`
        );
      }
    }
    //
    //  Create definitions for HASP terminals, if any
    //
    if (utilities.isInstalled("rbf5") && typeof netDevice.hasp !== "undefined") {
      ndlModset = ndlModset.concat(
      "",
      "****************************************************************",
      "*                                                              *",
      `*              HASP TERMINALS ON NP${toHex(netDevice.npuNode)}                          *`,
      "*                                                              *",
      "****************************************************************",
      ""
      );
      for (const termDef of netDevice.hasp) {
        let port = toHex(termDef.firstClaPort);
        ndlModset = ndlModset.concat(
        `LI${toHex(netDevice.npuNode)}P${port}: LINE      PORT=${port},LTYPE=S2,TIPTYPE=HASP.`,
        `         TERMINAL  TC=${termDef.termClass},RIC=YES.`,
        `         CO${toHex(netDevice.npuNode)}P${port}:  DEVICE,DT=CON,HN=${netDevice.couplerNode},PW=0,AUTOCON.`,
        `         CR${toHex(netDevice.npuNode)}P${port}:  DEVICE,DT=CR,STREAM=1.`,
        `         LP${toHex(netDevice.npuNode)}P${port}:  DEVICE,DT=LP,STREAM=1,SDT=A9,PW=132.`,
        `         PU${toHex(netDevice.npuNode)}P${port}:  DEVICE,DT=CP,STREAM=1.`
        );
      }
    }
    //
    //  Create definitions for TLF terminals, if any
    //
    if (utilities.isInstalled("tlf") && typeof netDevice.tlf !== "undefined") {
      ndlModset = ndlModset.concat(
      "",
      "****************************************************************",
      "*                                                              *",
      `*              REVERSE HASP TERMINALS ON NP${toHex(netDevice.npuNode)}                  *`,
      "*                                                              *",
      "****************************************************************"
      );
      for (const termDef of netDevice.tlf) {
        let port = toHex(termDef.firstClaPort);
        ndlModset = ndlModset.concat(
        "",
        `*  ${termDef.id}`,
        `LI${toHex(netDevice.npuNode)}P${port}: LINE      PORT=${port},LTYPE=S2,TIPTYPE=TT12.`,
        "         TERMINAL  TC=TC28,STIP=USER.",
        `         CO${toHex(netDevice.npuNode)}P${port}:  DEVICE,REVHASP,HN=${netDevice.couplerNode},DT=CON,ABL=2,DBZ=120,PW=120.`,
        `         CR${toHex(netDevice.npuNode)}P${port}:  DEVICE,REVHASP,HN=${netDevice.couplerNode},DT=CR,DO=1,TA=1,ABL=3,DBZ=810,`,
        "                          PW=137.",
        `         LP${toHex(netDevice.npuNode)}P${port}:  DEVICE,REVHASP,HN=${netDevice.couplerNode},DT=LP,DO=1,TA=1,ABL=0,DBZ=0.`,
        `         PL${toHex(netDevice.npuNode)}P${port}:  DEVICE,REVHASP,HN=${netDevice.couplerNode},DT=PL,DO=1,TA=1,ABL=0,DBZ=0.`
        );
      }
    }
    //
    //  Create definitions for NJE terminals, if any
    //
    if (utilities.isInstalled("njf") && typeof netDevice.nje !== "undefined") {
      ndlModset = ndlModset.concat(
      "",
      "****************************************************************",
      "*                                                              *",
      `*              NJE TERMINALS ON NP${toHex(netDevice.npuNode)}                           *`,
      "*                                                              *",
      "****************************************************************"
      );
      for (const termDef of netDevice.nje) {
        let port = toHex(termDef.firstClaPort);
        ndlModset = ndlModset.concat(
        "",
        `*  ${termDef.id}`,
        `LI${toHex(netDevice.npuNode)}P${port}: LINE      PORT=${port},LTYPE=S2,TIPTYPE=TT13.`,
        `         TE${toHex(netDevice.npuNode)}P${port}:  TERMDEV,STIP=USER,TC=TC29,DT=CON,ABL=7,`,
        "                           DBZ=1020,UBZ=11,DBL=7,UBL=12,XBZ=400,",
        `                           HN=${netDevice.couplerNode},AUTOCON.`
        );
      }
    }
  }
}

//
// Generate NPU, coupler, and logical link definitions for remote NPU's if any are defined
//
let remoteNpuKeys = Object.keys(remoteNpus);
if (remoteNpuKeys.length > 0) {
  for (const key of remoteNpuKeys) {
    let npu = remoteNpus[key];
    ndlModset = ndlModset.concat(
      "",
      "****************************************************************",
      "*                                                              *",
      `*                   DEFINITIONS FOR NP${toHex(npu.npuNode)}                       *`,
      "*                                                              *",
      `*   NP${toHex(npu.npuNode)} IS ATTACHED TO MAINFRAME ${npu.lid} VIA COUPLER CP${toHex(npu.couplerNode)}${npu.lid}.     *`,
      `*   COUPLER CP${toHex(npu.couplerNode)}${npu.lid} IS THE SOURCE NODE FOR A LOGICAL LINK TO   *`,
      `*   NPU NP${toHex(npu.npuNode)}.                                                  *`,
      "*                                                              *",
      "****************************************************************",
      "",
      `NP${toHex(npu.npuNode)}:    NPU,      NODE=${npu.npuNode},VARIANT=SM1,DMP=NO.`,
      `         SUPLINK,  LLNAME=LL${toHex(npu.couplerNode)}N${toHex(npu.npuNode)}.`,
      "",
      `CP${toHex(npu.couplerNode)}${npu.lid}: COUPLER,  NODE=${npu.couplerNode}.`,
      `         LL${toHex(npu.couplerNode)}N${toHex(npu.npuNode)}:  LOGLINK, NCNAME=NP${toHex(npu.npuNode)}.`
    );
    for (const node of topology) {
      if (node.id !== hostID && node.npuNode === npu.npuNode && node.couplerNode === npu.couplerNode) {
        for (const linkName of Object.keys(node.links)) {
          let link = node.links[linkName];
          let couplerNode = couplerMap[`${link.id}.${link.npuNode}`];
          let linkName1 = `LL${toHex(node.couplerNode)}C${toHex(couplerNode)}`;
          let linkName2 = `LL${toHex(couplerNode)}C${toHex(node.couplerNode)}`;
          if (logicalLinks.indexOf(linkName1) === -1 && logicalLinks.indexOf(linkName2) === -1) {
            logicalLinks.push(linkName1, linkName2);
            ndlModset.push(`         ${linkName1}:  LOGLINK, NCNAME=CP${toHex(couplerNode)}${lidMap[link.id]}.`);
          }
        }
      }
    }
  }
}

//
// Modify the LCFFILE declaration
//
ndlModset = ndlModset.concat(
  "*D 139",
  `*            LOCAL CONFIGURATION FILE FOR HOST M${mid}             *`,
  "*D 143,144",
  `LCFM${mid} : LFILE.`,
  `         TITLE,    LOCAL CONFIGURATION FOR HOST M${mid}.`
);

//
// Add a definition for the NJU application
//
if (utilities.isInstalled("njf")) {
  ndlModset = ndlModset.concat(
    "*I 163",
    "NJU:     APPL,     MXCOPYS=15."
  );
}

//
// Modify the TCP/IP Gateway OUTCALL definition if the coupler node, CDCNet node,
// or machine ID are changing.
//
if (typeof config["MDI0"] !== "undefined") {
  let node = config["MDI0"];
  if (mid !== "M01" || node.couplerNode !== 1 || node.cdcnetNode !== 255) {
    ndlModset = ndlModset.concat(
      "*D 181,182",
      `OUTCALL,NAME1=TCPIPGW,NAME2=H${mid},SNODE=${node.couplerNode},DNODE=${node.cdcnetNode},NETOSD=DDV,`,
      `        ABL=7,DBL=7,DBZ=2000,UBL=7,UBZ=18,SERVICE=GW_TCPIP_M${mid}.`
    );
  }
}
else {
  //
  //  Delete TCP/IP Gateway OUTCALL if MDI0 is not defined
  //
  ndlModset.push("*D 181,182");
}

//
// Generate INCALL and OUTCALL definitions for QTFS and PTFS if at least one other node is linked to the local node.
//
if (remoteNpuKeys.length > 0) {
  ndlModset = ndlModset.concat(
    "*D 184,185",
    "INCALL,FAM=0,UNAME=NETOPS,ANAME=QTFS,DBL=7,ABL=7,DBZ=1024."
  );
  for (const node of topology) {
    if (node.id === hostID) {
      for (const key of Object.keys(node.links)) {
        let peer = node.links[key];
        ndlModset.push(`OUTCALL,NAME1=QTFS,PID=${lidMap[peer.id]},SNODE=${node.couplerNode},DNODE=${couplerMap[`${peer.id}.${peer.npuNode}`]},DBL=7,ABL=7,DBZ=1024.`);
      }
    }
  }
  ndlModset = ndlModset.concat(
    "*D 187,188",
    "INCALL,FAM=0,UNAME=NETOPS,ANAME=PTFS,DBL=7,ABL=7,DBZ=1024."
  );
  for (const node of topology) {
    if (node.id === hostID) {
      for (const key of Object.keys(node.links)) {
        let peer = node.links[key];
        ndlModset.push(`OUTCALL,NAME1=PTFS,PID=${lidMap[peer.id]},SNODE=${node.couplerNode},DNODE=${couplerMap[`${peer.id}.${peer.npuNode}`]},DBL=7,ABL=7,DBZ=1024.`);
      }
    }
  }
}

//
// Modify USER definitions
//
if (utilities.isInstalled("cybis-base")) {
  ndlModset = ndlModset.concat(
    "*I 196",
    "CYBUSER: DEFINE,   MFAM=CYBER,MUSER=SYS,MAPPL=CYBIS."
  );
}
ndlModset.push("*D 200,230");
for (const key of configKeys) {
  if (key.startsWith("MDI") || key.startsWith("NPU")) {
    let netDevice = config[key];
    if (typeof netDevice.raw !== "undefined") {
      ndlModset.push("");
      for (const termDef of netDevice.raw) {
        for (let i = 0; i < termDef.claPortCount; i++) {
          ndlModset.push(`PR${toHex(netDevice.npuNode)}P${toHex(termDef.firstClaPort + i)}: USER,     ${termDef.user}.`);
        }
      }
    }
    if (typeof netDevice.telnet !== "undefined") {
      ndlModset.push("");
      for (const termDef of netDevice.telnet) {
        for (let i = 0; i < termDef.claPortCount; i++) {
          ndlModset.push(`TE${toHex(netDevice.npuNode)}P${toHex(termDef.firstClaPort + i)}: USER,     ${termDef.user}.`);
        }
      }
    }
    if (utilities.isInstalled("cybis-base") && typeof netDevice.pterm !== "undefined") {
      ndlModset.push("");
      for (const termDef of netDevice.pterm) {
        for (let i = 0; i < termDef.claPortCount; i++) {
          ndlModset.push(`TE${toHex(netDevice.npuNode)}P${toHex(termDef.firstClaPort + i)}: USER,     ${termDef.user}.`);
        }
      }
    }
    if (utilities.isInstalled("rbf5") && typeof netDevice.hasp !== "undefined") {
      ndlModset.push("");
      for (const termDef of netDevice.hasp) {
        for (let i = 0; i < termDef.claPortCount; i++) {
          ndlModset.push(`CO${toHex(netDevice.npuNode)}P${toHex(termDef.firstClaPort + i)}: USER,     ${termDef.user}.`);
        }
      }
    }
    if (utilities.isInstalled("tlf") && typeof netDevice.tlf !== "undefined") {
      ndlModset.push("");
      for (const termDef of netDevice.tlf) {
        for (let i = 0; i < termDef.claPortCount; i++) {
          ndlModset.push(`CO${toHex(netDevice.npuNode)}P${toHex(termDef.firstClaPort + i)}: USER,     ${termDef.user}.`);
        }
      }
    }
    if (utilities.isInstalled("njf") && typeof netDevice.nje !== "undefined") {
      ndlModset.push("");
      for (const termDef of netDevice.nje) {
        for (let i = 0; i < termDef.claPortCount; i++) {
          ndlModset.push(`TE${toHex(netDevice.npuNode)}P${toHex(termDef.firstClaPort + i)}: USER,     ${termDef.user}.`);
        }
      }
    }
  }
}
ndlModset = ndlModset.concat(
  "*/",
  "*/ END Of MODSET"
);

//for (const line of ndlModset) process.stdout.write(`${line}\n`);

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => utilities.updateNDL(dtc, ndlModset))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
