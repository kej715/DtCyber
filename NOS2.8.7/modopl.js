#!/usr/bin/env node

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

dtc.say("Load system source tapes ...")
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[UNLOAD,52.",
  "[UNLOAD,53."
]))
.then(() => dtc.mount(13, 0, 1, "tapes/nos287-1.tap"))
.then(() => dtc.mount(13, 0, 2, "tapes/nos287-2.tap"))
.then(() => dtc.mount(13, 0, 3, "tapes/nos287-3.tap"))
.then(() => dtc.say("Run job to load and update OPL871 with local mods ..."))
.then(() => dtc.runJob(12, 4, "decks/modopl.job"))
.then(() => dtc.say("Unload system source tapes ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[UNLOAD,52.",
  "[UNLOAD,53."
]))
.then(() => dtc.say("MODOPL completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(`Error caught: ${err}`);
  process.exit(1);
});

