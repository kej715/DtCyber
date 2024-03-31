#!/usr/bin/env node

const DtCyber = require("../automation/DtCyber");
const fs      = require("fs");

const dtc = new DtCyber();

const nosTapes = ["nos287-1.tap", "nos287-2.tap", "nos287-3.tap"];

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Load system source tapes ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[UNLOAD,52.",
  "[UNLOAD,53.",
  "[!"
]))
.then(() => {
  let promise = Promise.resolve();
  let unit = 1;
  for (const nosTape of nosTapes) {
    if (!fs.existsSync(`tapes/${nosTape}`)) {
      promise = promise
      .then(() => dtc.say(`Decompress tapes/${nosTape}.bz2 to tapes/${nosTape} ...`))
      .then(() => dtc.bunzip2(`tapes/${nosTape}.bz2`, `tapes/${nosTape}`));
    }
    promise = promise
    .then(() => dtc.mount(13, 0, unit++, `tapes/${nosTape}`))
  }
  return promise;
})
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

