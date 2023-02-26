#!/usr/bin/env node
//
// Update the cyber.ovl file to include NJE terminal definitions.
//

const DtCyber   = require("../../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const hostID      = utilities.getHostId(dtc);
const npuNode     = utilities.getNpuNode(dtc);
const topology    = utilities.getNjeTopology(dtc);

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

//
// If an overlay file exists, read it, and merge its definitions with
// definitions generated from customization properties and adjacent node
// definitions, and create a new overlay file.
//
let ovlProps = {};
let ovlPath  = "cyber.ovl";
if (fs.existsSync(ovlPath)) {
  dtc.readPropertyFile(ovlPath, ovlProps);
}
else if (fs.existsSync("../cyber.ovl")) {
  ovlPath = "../cyber.ovl";
  dtc.readPropertyFile(ovlPath, ovlProps);
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
const localNode      = topology[hostID];
let   defaultLocalIP = null;
if (typeof localNode.publicAddress !== "undefined") {
  let parts = localNode.publicAddress.split(":");
  if (parts[0] !== "0.0.0.0") {
    defaultLocalIP = parts[0];
  }
}
names = Object.keys(utilities.getAdjacentNjeNodes(dtc)).sort();
for (const name of names) {
  let node       = topology[name];
  let localIP    = defaultLocalIP;
  let listenPort = 175;
  if (typeof node.localAddress !== "undefined") {
    let parts = node.localAddress.split(":");
    localIP   = parts[0];
    if (parts.length > 1) {
      listenPort = parts[1];
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

fs.writeFileSync(ovlPath, lines.join("\n"));
