#!/usr/bin/env node

const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Load tapes ..."))
.then(() => dtc.dsd([
  "[UNLOAD,50.",
  "[UNLOAD,51.",
  "[!"
]))
.then(() => dtc.sleep(3000))
.then(() => dtc.mount(13, 0, 0, "tapes/ds.tap"))
.then(() => dtc.mount(13, 0, 1, "tapes/newds.tap", true))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Run job to write new deadstart tape ..."))
.then(() => dtc.runJob(12, 4, "decks/make-ds-tape.job", [50, 51]))
.then(() => dtc.say("New deadstart tape created: tapes/newds.tap"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
