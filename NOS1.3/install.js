#!/usr/bin/env node

const fs      = require("fs");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

let promise = dtc.say("Begin installation of NOS 1.3 ...");
for (const baseTape of ["ds.tap", "opl485.tap"]) {
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
.then(() => dtc.attachPrinter("LP5xx_C11_E5"))
.then(() => dtc.dsd([
  "",
  "]!",
  "INITIALIZE,1,AL.",
  "INITIALIZE,2,AL.",
  "INITIALIZE,3,AL.",
  "INITIALIZE,4,AL.",
  "INITIALIZE,11,AL.",
  "INITIALIZE,12,AL.",
  "INITIALIZE,13,AL.",
  "INITIALIZE,14,AL.",
  "INITIALIZE,17,AL.",
  "GO.",
  "#1500#%year%%mon%%day%",
  "#1500#%hour%%min%%sec%"
]))
.then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
.then(() => dtc.say("Initial deadstart complete"))
.then(() => dtc.say("Create VALIDUZ in default family ..."))
.then(() => dtc.dis([
  "MODVAL,OP=C.",
  "DEFINE,VI=VALINDZ,VU=VALIDUZ.",
  "REWIND,NEWVAL,VALINDZ.",
  "COPY,NEWVAL,VU.",
  "COPY,VALINDZ,VI.",
  "CLEAR.",
  "ISF."
]))
.then(() => dtc.say("Create user INSTALL ..."))
.then(() => dtc.dis([
  "MODVAL,OP=Z./INSTALL,PW=INSTALL,FUI=1,AW=ALL,MT=7",
  "MODVAL,OP=Z./INSTALL,RP=7,SC=77B,CM=77B,EC=77B",
  "MODVAL,OP=Z./INSTALL,OF=7,DB=7,DS=7,FS=7,FC=7",
  "MODVAL,OP=Z./INSTALL,CS=7,CP=77B,CC=77B,DF=77B"
]))
.then(() => dtc.say("Copy OPL485 to INSTALL catalog ..."))
.then(() => dtc.mount(13, 0, 1, "tapes/opl485.tap"))
.then(() => dtc.runJob(11, 4, "decks/create-opl485.job", [51]))
.then(() => dtc.say("Make a copy of SYSTEM to support customization ..."))
.then(() => dtc.dis([
  "COMMON,SYSTEM.",
  "DEFINE,SYS=SYSTEM.",
  "COPY,SYSTEM,SYS."
], 1))
.then(() => dtc.say("Create user GUEST ..."))
.then(() => dtc.dis("MODVAL,OP=Z./GUEST,PW=GUEST,FUI=32B,AW=CCNR,SC=77B"))
.then(() => dtc.say("Create VALIDUZ in family PLATO ..."))
.then(() => dtc.dis([
  "FAMILY,PLATO.",
  "MODVAL,FM=PLATO,OP=C.",
  "DEFINE,VI=VALINDZ,VU=VALIDUZ.",
  "REWIND,NEWVAL,VALINDZ.",
  "COPY,NEWVAL,VU.",
  "COPY,VALINDZ,VI.",
  "CLEAR.",
  "ISF."
]))
.then(() => dtc.say("Create user PLATO in family PLATO ..."))
.then(() => dtc.dis([
  "FAMILY,PLATO.",
  "MODVAL,FM=PLATO,OP=Z./PLATO,PW=PLATO,FUI=1,AW=ALL",
  "MODVAL,FM=PLATO,OP=Z./PLATO,MT=7,RP=7,SC=77B",
  "MODVAL,FM=PLATO,OP=Z./PLATO,CM=77B,EC=77B,OF=7",
  "MODVAL,FM=PLATO,OP=Z./PLATO,DB=7,DS=7,FS=7,FC=7",
  "MODVAL,FM=PLATO,OP=Z./PLATO,CS=7,CP=77B,CC=77B",
  "MODVAL,FM=PLATO,OP=Z./PLATO,DF=77B"
]))
.then(() => dtc.say("Get PLATO PFDUMP tape ..."))
.then(() => dtc.wget("https://www.dropbox.com/s/q8v19dxqjfr7j2w/plato4nos13.tap.bz2?dl=1", "tapes"))
.then(() => dtc.say("Decompress PLATO PFDUMP tape ..."))
.then(() => dtc.bunzip2("tapes/plato4nos13.tap.bz2", "tapes/plato4nos13.tap"))
.then(() => dtc.say("Load PLATO PFDUMP tape on tape drive ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[!"
]))
.then(() => dtc.sleep(2000))
.then(() => dtc.mount(13, 0, 1, "tapes/plato4nos13.tap"))
.then(() => dtc.say("Create job to restore PLATO master files ..."))
.then(() => dtc.runJob(11, 4, "decks/upload-platomf-pfload.job"))
.then(() => dtc.say("Run job to restore PLATO master files ..."))
.then(() => dtc.dis([
  "GET,PFLPLMF.",
  "ROUTE,PFLPLMF,DC=IN,OT=SYOT."
], 1))
.then(() => dtc.waitJob("PFLPLMF"))
.then(() => dtc.say("Get PLATO development tools PFDUMP tape ..."))
.then(() => dtc.wget("https://www.dropbox.com/s/0dyv29cmf4rse9t/platodv4nos13.tap.bz2?dl=1", "tapes"))
.then(() => dtc.say("Decompress PLATO development tools PFDUMP tape ..."))
.then(() => dtc.bunzip2("tapes/platodv4nos13.tap.bz2", "tapes/platodv4nos13.tap"))
.then(() => dtc.say("Load PLATO development tools PFDUMP tape on tape drive ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[!"
]))
.then(() => dtc.sleep(2000))
.then(() => dtc.mount(13, 0, 1, "tapes/platodv4nos13.tap"))
.then(() => dtc.say("Create job to restore PLATO development tools ..."))
.then(() => dtc.runJob(11, 4, "decks/upload-platodv-pfload.job"))
.then(() => dtc.say("Run job to restore PLATO development tools ..."))
.then(() => dtc.dis([
  "GET,PFLPLDV.",
  "ROUTE,PFLPLDV,DC=IN,OT=SYOT."
], 1))
.then(() => dtc.waitJob("PFLPLDV"))
.then(() => dtc.sleep(4000))
.then(() => dtc.shutdown(false))
.then(() => dtc.say("Re-deadstart freshly installed system ..."))
.then(() => dtc.start({
  detached: true,
  stdio: [0, "ignore", 2],
  unref: false
}))
.then(() => dtc.sleep(5000))
.then(() => dtc.attachPrinter("LP5xx_C11_E5"))
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.say("Deadstart complete"))
.then(() => dtc.connect())
.then(() => dtc.expect([{ re: /Operator> $/ }]))
.then(() => dtc.say("Installation of NOS 1.3 complete"))
.then(() => dtc.say("Enter 'exit' command to exit and shutdown gracefully"))
.then(() => dtc.engageOperator())
.then(() => dtc.shutdown())
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
