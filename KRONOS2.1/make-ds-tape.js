#!/usr/bin/env node

const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.attachPrinter("../KRONOS2.1/LP5xx_C12_E6"))
.then(() => dtc.say("Mount new deadstart tape"))
.then(() => dtc.mount(13, 0, 1, "tapes/newds.tap", true))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Run job to write new deadstart tape"))
.then(() => dtc.runJob(12, 4, "decks/make-ds-tape.job", [51]))
.then(() => dtc.say("New deadstart tape created: tapes/newds.tap"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
