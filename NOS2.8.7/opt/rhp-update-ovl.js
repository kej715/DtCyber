#!/usr/bin/env node
//
// Update the cyber.ovl file to reflect RHP topology (e.g., trunk definitions)
//

const DtCyber   = require("../../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const mid         = utilities.getMachineId(dtc);
const hostID      = utilities.getHostId(dtc);
const npuNode     = utilities.getNpuNode(dtc);
const topology    = utilities.getRhpTopology(dtc);

const localNode   = topology[hostID];

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

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
  let node = topology[nodeName];
  ovlText.push(`terminals=${localPort},0x${toHex(localNode.links[nodeName])},1,trunk,${node.addr},${node.id},${node.couplerNode}`);
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
fs.writeFileSync("cyber.ovl", ovlLines.join("\n"));
