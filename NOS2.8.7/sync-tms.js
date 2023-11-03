#!/usr/bin/env node

const fs = require("fs");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

const props = dtc.getIniProperties(dtc);
const equipment = props["equipment.nos287"];
let tapeHost = "127.0.0.1";
for (const line of iniProps["equipment.nos287"]) {
  if (line.startsWith("MT5744,")) {
    let tokens = line.split(",");
    let ci     = tokens[4].indexOf(":");
    tapeHost   = tokens[4].substring(0, ci);
  }
}

if (fs.existsSync("files/TMS-catalog.txt")) {
  fs.unlinkSync("files/TMS-catalog.txt");
}

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Get the initial TMS catalog definition from StorageTek 4400 simulator ..."))
.then(() => dtc.wget(`http://${tapeHost}:4480/volumes?tfsp`, "files", "TMS-catalog.txt"))
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
