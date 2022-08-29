#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.start({
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
})
.then(() => dtc.say("DtCyber started - deadstarting SCOPE/HUSTLER"))
.then(() => dtc.sleep(2000))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.say("Enter 'exit' command to exit and terminate system gracefully"))
.then(() => dtc.engageOperator(cmdExtensions))
.then(() => dtc.dsd([
  "UNL#500#OP #500#",
  "BLITZ.",
  "#2000#STEP."
]))
.then(() => dtc.sleep(2000))
.then(() => dtc.send("shutdown"))
.then(() => dtc.expect([{ re: /Goodbye for now/ }]))
.then(() => dtc.say("Shutdown complete"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
