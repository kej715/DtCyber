#!/usr/bin/env node
//
// Update the NDL to reflect the RHP topology.
//

const DtCyber   = require("../../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./utilities");

const dtc = new DtCyber();

const customProps = utilities.getCustomProperties(dtc);
const couplerNode = utilities.getCouplerNode(dtc);

//
// Determine current coupler node
//
const iniProps = dtc.getIniProperties(dtc);
let currentCouplerNode = 1;
if (typeof iniProps["npu.nos287"] !== "undefined") {
  for (const line of iniProps["npu.nos287"]) {
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim().toUpperCase();
    let value = line.substring(ei + 1).trim();
    if (key === "COUPLERNODE") {
      currentCouplerNode = parseInt(value);
    }
  }
}

//
// If the coupler node is changing, the equipment deck needs
// to be updated.
//
if (currentCouplerNode !== couplerNode) {
  dtc.connect()
  .then(() => dtc.expect([ {re:/Operator> $/} ]))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.say("Update NPU/MDI coupler node declaration in EQPD01 ..."))
  .then(() => utilities.getSystemRecord(dtc, "EQPD01"))
  .then(eqpd01 => {
    let si = eqpd01.search(/EQ[0-7]+=ND/);
    if (si === -1) {
      si = eqpd01.search(/EQ[0-7]+=NP/);
      if (si === -1) return dtc.say("  WARNING: no NPU/MDI equipment definition found");
    }
    let ei = eqpd01.indexOf("\n", si);
    let eqDefn = eqpd01.substring(si, ei);
    eqDefn = eqDefn.replace(/ND=[0-7]+/, `ND=${couplerNode.toString(8)}`);
    eqpd01 = `${eqpd01.substring(0, si)}${eqDefn}${eqpd01.substring(ei)}`;
    const job = [
      "$ATTACH,PRODUCT/M=W,WB.",
      "$COPY,INPUT,EQPD.",
      "$REWIND,EQPD.",
      "$LIBEDIT,P=PRODUCT,B=EQPD,I=0,C."
    ];
    const options = {
      jobname: "EQPDUPD",
      username: "INSTALL",
      password: utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL"),
      data:    eqpd01
    };
    return dtc.createJobWithOutput(12, 4, job, options);
  })
  .then(() => {
    process.exit(0);
  })
  .catch(err => {
    console.log(err);
    process.exit(1);
  });
}
