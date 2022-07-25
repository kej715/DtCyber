#!/usr/bin/env node

const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.shutdown())
.then(() => dtc.expect([ {re:/Goodbye/} ]))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
