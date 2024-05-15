#!/usr/bin/env terminal
//
// Add HASP terminal definitions to cyber.ovl.
//

const DtCyber   = require("../../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const hostID      = utilities.getHostId(dtc);
const npuNode     = utilities.getNpuNode(dtc);
const terminals   = utilities.getHaspTerminals(dtc);

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

//
// If an overlay file exists, read it, and merge its definitions with
// definitions generated from customization properties and HASP terminal
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
    if (key !== "TERMINALS" || value.toUpperCase().indexOf(",HASP,") < 0) {
      ovlText.push(line);
    }
  }
}
if (terminals.length > 0) {
  i = 0;
  while (i < terminals.length) {
    let terminal  = terminals[i++];
    let claPort   = terminal.claPort;
    if (typeof terminal.blockSize === "undefined") terminal.blockSize = 400;
    let count     = 1;
    while (i < terminals.length) {
      let nextTerminal = terminals[i];
      if (typeof nextTerminal.blockSize === "undefined") nextTerminal.blockSize = 400;
      if (terminal.tcpPort === nextTerminal.tcpPort
          && (claPort + 1) === nextTerminal.claPort
          && terminal.blockSize === nextTerminal.blockSize) {
        claPort += 1;
        count   += 1;
        i       += 1;
      }
      else {
        break;
      }
    }
    let termDefn = `terminals=${terminal.tcpPort},0x${toHex(terminal.claPort)},${count},hasp,B${terminal.blockSize}`;
    ovlText.push(termDefn);
  }
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
