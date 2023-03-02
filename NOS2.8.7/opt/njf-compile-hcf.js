#!/usr/bin/env node
//
// Compile the NJF Host Configuration File and move the compiled
// object to SYSTEMX.

const DtCyber   = require("../../automation/DtCyber");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Compile NJF host configuration file ..."))
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
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
