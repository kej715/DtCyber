#!/usr/bin/env node

const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
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
