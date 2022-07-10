#!/usr/bin/env node

const fs = require("fs");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

let dtCyberPath = null;
for (const path of ["./dtcyber", "../dtcyber", "../bin/dtcyber"]) {
  if (fs.existsSync(path)) {
    dtCyberPath = path;
    break;
  }
}
if (dtCyberPath === null) {
  process.stderr,write("DtCyber executable not found in current directory or parent directory\n");
  process.exit(1);
}

let promise = Promise.resolve();
for (const baseTape of ["ds.tap", "nos287-1.tap", "nos287-2.tap", "nos287-3.tap"]) {
  if (!fs.existsSync(`tapes/${baseTape}`)) {
    promise = promise
    .then(() => dtc.say(`Decompress tapes/${baseTape}.bz2 to tapes/${baseTape} ...`))
    .then(() => dtc.bunzip2(`tapes/${baseTape}.bz2`, `tapes/${baseTape}`));
  }
}

promise = promise
.then(() => dtc.start(dtCyberPath, ["manual"]))
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("DtCyber started"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Begin initial deadstart ..."))
.then(() => dtc.dsd([
  "O!",
  "#500#P!",
  "#500#D=YES",
  "#500#",
  "#1000#NEXT.",
  "#1000#]!",
  "#1000#INITIALIZE,AL,5,6,10,11,12,13.",
  "#500#GO.",
  "#1500#%year%%mon%%day%",
  "#1500#%hour%%min%%sec%"
]))
.then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
.then(() => dtc.say("Initial deadstart complete"))
.then(() => dtc.say("Start SYSGEN(FULL) ..."))
.then(() => dtc.mount(13, 0, 1, "tapes/nos287-1.tap"))
.then(() => dtc.mount(13, 0, 2, "tapes/nos287-2.tap"))
.then(() => dtc.mount(13, 0, 3, "tapes/nos287-3.tap"))
.then(() => dtc.dsd("X.SYSGEN(FULL)"))
.then(() => dtc.expect([ {re:/E N D   F U L L/} ], "printer"))
.then(() => dtc.say("SYSGEN(FULL) complete"))
.then(() => dtc.say("Create and compile the NDL file ..."))
.then(() => dtc.runJob(12, 4, "decks/create-ndlopl.job"))
.then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job"))
.then(() => dtc.say("Update the NAMSTRT file ..."))
.then(() => dtc.dis("PERMIT,NAMSTRT,INSTALL=W.", 377772))
.then(() => dtc.runJob(12, 4, "decks/update-namstrt.job"))
.then(() => dtc.say("Create the TCPHOST file ..."))
.then(() => dtc.runJob(12, 4, "decks/create-tcphost.job"))
.then(() => dtc.say("Add terminal definitions to TERMLIB ..."))
.then(() => dtc.dis("PERMIT,TERMLIB,INSTALL=W.", 377776))
.then(() => dtc.runJob(12, 4, "decks/update-termlib.job"))
.then(() => dtc.say("Add a non-privileged GUEST user ..."))
.then(() => dtc.dsd("X.MS(VALUSER,LIMITED,GUEST,GUEST)"))
.then(() => dtc.say("Start SYSGEN(SOURCE) ..."))
.then(() => dtc.dsd("[X.SYSGEN(SOURCE)"))
.then(() => dtc.expect([ {re:/E N D   S O U R C E/} ], "printer"))
.then(() => dtc.say("SYSGEN(SOURCE) complete"))
.then(() => dtc.say("Run job to initialize customization artifacts ..."))
.then(() => dtc.runJob(12, 4, "decks/init-build.job"))
.then(() => dtc.say("Run job to load and update OPL871 ..."))
.then(() => dtc.runJob(12, 4, "decks/modopl.job"))
.then(() => dtc.say("Unload system source tapes ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[UNLOAD,52.",
  "[UNLOAD,53."
]))
.then(() => dtc.say("Base installation completed successfully"))
.then(() => dtc.shutdown())
.catch(err => {
  console.log(`Error caught: ${err}`);
  process.exit(1);
});
