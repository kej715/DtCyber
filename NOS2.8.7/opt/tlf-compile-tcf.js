#!/usr/bin/env node
//
// Compile the TLF configuration file and move the compiled object
// to NETADMN.
//

const DtCyber   = require("../../automation/DtCyber");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Compile TLF configuration file ..."))
.then(() => dtc.runJob(12, 4, "opt/tlf-compile-tcf.job"))
.then(() => dtc.say("Move TCFFILE to NETADMN ..."))
.then(() => dtc.dis([
  "GET,TCFFILE.",
  "PURGE,TCFFILE.",
  "SUI,25.",
  "PURGE,TCFFILE/NA.",
  "SAVE,TCFFILE/CT=PU,AC=N,M=R."
], "MOVETCF", 1))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
