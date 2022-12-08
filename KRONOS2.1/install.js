#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");

const dtc = new DtCyber();

let promise = dtc.say("Begin installation of KRONOS 2.1 ...");
for (const baseTape of ["ds.tap", "opl404.tap"]) {
  if (!fs.existsSync(`tapes/${baseTape}`)) {
    promise = promise
    .then(() => dtc.say(`Decompress tapes/${baseTape}.bz2 to tapes/${baseTape} ...`))
    .then(() => dtc.bunzip2(`tapes/${baseTape}.bz2`, `tapes/${baseTape}`));
  }
}

promise = promise
.then(() => dtc.start(["manual"]))
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("DtCyber started"))
.then(() => dtc.attachPrinter("LP5xx_C12_E6"))
.then(() => dtc.dsd([
  "",
  "]!",
  "INITIALIZE,0,AL.",
  "INITIALIZE,1,AL.",
  "GO.",
  "#1500#%year%%mon%%day%",
  "#1500#%hour%%min%%sec%"
]))
.then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
.then(() => dtc.say("Initial deadstart complete"))
.then(() => dtc.say("Create VALIDUS with INSTALL user in default family ..."))
.then(() => dtc.dis([
  "MODVAL,OP=C.",
  "DEFINE,VI=VALINDS,VU=VALIDUS.",
  "REWIND,NEWVAL,VALINDS.",
  "COPY,NEWVAL,VU.",
  "COPY,VALINDS,VI.",
  "CLEAR.",
  "ISF.",
  "MODVAL,OP=Z./INSTALL,PW=INSTALL,FUI=1,AW=ALL,DB=1"
]))
.then(() => dtc.say("Update INSTALL user and create GUEST user ..."))
.then(() => dtc.runJob(12, 4, "decks/create-users.job"))
.then(() => dtc.dis([
  "GET,USERS.",
  "MODVAL,FA,I=USERS,OP=U.",
  "PURGE,USERS/NA."
], 1))
.then(() => dtc.sleep(1000))
.then(() => dtc.say("Apply mods to OPL404, and copy it to INSTALL catalog ..."))
.then(() => dtc.mount(13, 0, 1, "tapes/opl404.tap"))
.then(() => dtc.runJob(12, 4, "decks/create-opl404.job", [51]))
.then(() => dtc.say("Installation of KRONOS 2.1 complete"))
.then(() => dtc.say("Enter 'exit' command to exit and shutdown gracefully"))
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
  process.stdout.write("Press ENTER to terminate...");
  process.stdin.on("data", data => {
    process.exit(1);
  });
  process.stdin.on("close", () => {
    process.exit(1);
  });
});
