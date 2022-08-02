#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.start({
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
})
.then(() => dtc.say("DtCyber started - deadstarting NOS 2.8.7"))
.then(() => dtc.sleep(2000))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.say("Deadstart complete"))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.say("Enter 'exit' command to exit and terminate system gracefully"))
.then(() => dtc.engageOperator(cmdExtensions))
.then(() => dtc.shutdown())
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
