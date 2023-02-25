#!/usr/bin/env node
//
// Compile NDL source file
//

const DtCyber   = require("../../automation/DtCyber");
const utilities = require("./utilities");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => utilities.compileNDL(dtc))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
