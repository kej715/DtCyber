#!/usr/bin/env node

const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");

const dtc = new DtCyber();

let promise = dtc.say("Starting Chippewa Operating System (COS) ...");
for (const baseTape of ["ds.tap"]) {
  if (!fs.existsSync(`tapes/${baseTape}`)) {
    promise = promise
    .then(() => dtc.say(`Decompress tapes/${baseTape}.bz2 to tapes/${baseTape} ...`))
    .then(() => dtc.bunzip2(`tapes/${baseTape}.bz2`, `tapes/${baseTape}`));
  }
}
promise = promise
.then(() => dtc.start({
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
}))
.then(() => dtc.sleep(1000))
.then(() => dtc.attachPrinter("LP1612_C13"))
.then(() => dtc.expect([{ re: /STARTUP COMPLETE/ }], "printer"))
.then(() => dtc.say("Deadstart complete"))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("COS started"))
.then(() => dtc.say("Enter 'exit' or 'shutdown' command to exit and shutdown gracefully"))
.then(() => dtc.engageOperator())
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
