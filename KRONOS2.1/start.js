#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.start({
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
})
.then(() => dtc.say("DtCyber started - deadstarting KRONOS 2.1"))
.then(() => dtc.sleep(2000))
.then(() => dtc.attachPrinter("LP5xx_C12_E6"))
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.say("Deadstart complete"))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.say("Enter 'exit' command to exit and terminate system gracefully"))
.then(() => dtc.engageOperator(cmdExtensions))
.then(() => dtc.say("Starting shutdown sequence ..."))
.then(() => dtc.dsd("[UNLOCK."))
.then(() => dtc.dsd("[BLITZ."))
.then(() => dtc.sleep(5000))
.then(() => dtc.dsd("CHECK#2000#"))
.then(() => dtc.sleep(2000))
.then(() => dtc.dsd("STEP."))
.then(() => dtc.sleep(1000))
.then(() => dtc.send("shutdown"))
.then(() => dtc.expect([{ re: /Goodbye for now/ }]))
.then(() => dtc.say("Shutdown complete ..."))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
