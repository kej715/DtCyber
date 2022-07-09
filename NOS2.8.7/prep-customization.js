#!/usr/bin/env node

const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.connect(6666)
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Load system source tapes ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[UNLOAD,52.",
  "[UNLOAD,53."
]))
.then(() => dtc.mount(13, 0, 1, "tapes/nos287-1.tap"))
.then(() => dtc.mount(13, 0, 2, "tapes/nos287-2.tap"))
.then(() => dtc.mount(13, 0, 3, "tapes/nos287-3.tap"))
.then(() => dtc.say("Start SYSGEN(SOURCE) ..."))
.then(() => dtc.dsd("[X.SYSGEN(SOURCE)"))
.then(() => dtc.expect([ {re:/E N D   S O U R C E/} ], "printer"))
.then(() => dtc.say("SYSGEN complete"))
.then(() => dtc.say("Run job to initialize customization artifacts ..."))
.then(() => dtc.runJob(12, 4, "decks/init-build.job"))
.then(() => dtc.say("Run job to initialize OPL871 ..."))
.then(() => dtc.runJob(12, 4, "decks/modopl.job"))
.then(() => dtc.say("Unload system source tapes ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[UNLOAD,52.",
  "[UNLOAD,53."
]))
.then(() => dtc.say("Prep for customization complete"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
