#!/usr/bin/env node

const fs = require("fs");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

fs.unlinkSync("files/TMS-catalog.txt");
dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Get the initial TMS catalog definition from Stk simulator ..."))
.then(() => dtc.wget("http://localhost:4480/volumes?tfsp", "files", "TMS-catalog.txt"))
.then(() => dtc.say("Run job to load the catalog ..."))
.then(() => dtc.runJob(12, 4, "decks/load-tms-catalog.job"))
.then(() => dtc.say("TMS catalog sync complete"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
