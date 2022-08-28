#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");

const dtc = new DtCyber();

if (!fs.existsSync("persistence")) {
  fs.mkdirSync("persistence");
}
let promise = dtc.say("Begin installation of SCOPE/HUSTLER ...");
if (!fs.existsSync("hustler.zip")) {
  promise = promise
  .then(() => dtc.say("Get HUSTLER media archive ..."))
  .then(() => dtc.wget("https://www.dropbox.com/s/4jjda7swz6odsvk/hustler.zip?dl=1", ".", "hustler.zip.download"))
  .then(() => new Promise((resolve, reject) => {
    fs.rename("hustler.zip.download", "hustler.zip", () => {
      resolve();
    });
  }));
}
if (!fs.existsSync("disks")) {
  promise = promise
  .then(() => dtc.say("Expand HUSTLER media archive ..."))
  .then(() => dtc.unzip("hustler.zip", "."));
}
for (const file of [
  "disks/PACK015.DSK",
  "disks/PACK032.DSK",
  "disks/PACK033.DSK",
  "disks/PACK034.DSK",
  "disks/PACK035.DSK",
  "disks/PACK036.DSK",
  "tapes/sys400a.tap"]) {
  if (!fs.existsSync(file)) {
    promise = promise
    .then(() => dtc.say(`Decompress ${file}.bz2 to ${file} ...`))
    .then(() => dtc.bunzip2(`${file}.bz2`, `${file}`));
  }
}

promise = promise
.then(() => dtc.start(["manual"]))
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("DtCyber started"))
.then(() => dtc.say("Initiate cold deadstart of SCOPE/HUSTLER"))
.then(() => dtc.dsd([
  "#2000#",
  "#1000#1.N",
  "#1000#2.N",
  "#1000#",
  "#1000#1.N",
  "#1000#3.C",
  "#1000#",
  "#2000#GO",
  "#20000#OFF",
  "#45000#D%mon%%day%96",
  "#1500#T%hour%%min%%sec%",
  "#1000#3.GO",
  "#1000#ATT.",
  "#1000#OFFAAF.",
  "#1000#OFFAD",
  "#1000#ONEI2",
  "#1000#ENJ#500#AU00001",
  "#1000#1.X MANAGER,NOLOAD",
  "#1000#2.X ARGUS",
  "#1000#3.N",
  "#1000#4.N",
  "#1000#5.N",
  "#1000#6.N",
  "#1000#7.N"
]))
.then(() => dtc.console("set_operator_port 6666"))
.then(() => dtc.say("Cold deadstart complete"))
.then(() => dtc.say("Enter 'exit' command to exit and shutdown gracefully"))
.then(() => dtc.engageOperator(cmdExtensions))
.then(() => dtc.dsd([
  "UNL#500#OP #500#",
  "BLITZ.",
  "#2000#STEP."
]))
.then(() => dtc.sleep(2000))
.then(() => dtc.send("shutdown"))
.then(() => dtc.expect([{ re: /Goodbye for now/ }]))
.then(() => dtc.say("Shutdown complete"))
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
