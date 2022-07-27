#!/usr/bin/env node

const fs = require("fs");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

let isBasicInstall = false;
let isContinueInstall = false;

for (let arg of process.argv.slice(2)) {
  arg = arg.toLowerCase();
  if (arg === "basic") {
    isBasicInstall = true;
  }
  else if (arg === "cont" || arg === "continue") {
    isContinueInstall = true;
  }
  else if (arg === "full") {
    isBasicInstall = false;
  }
  else {
    process.stderr.write(`Unrecognized argument: ${arg}\n`);
    process.stderr.write("Usage: node install [basic | full][continue]\n");
    process.exit(1);
  }
}

//
// Empty the tape image cache in case a download failure occurred during
// a previous run and one or more tape images are incomplete.
//
for (const name of fs.readdirSync("opt/tapes")) {
  if (name.toLowerCase() !== "dummy") fs.unlinkSync(`opt/tapes/${name}`);
}

let installedProductSet = [];
if (fs.existsSync("opt/installed.json")) {
  if (isContinueInstall) {
    installedProductSet = JSON.parse(fs.readFileSync("opt/installed.json", "utf8"));
  }
  else {
    fs.unlinkSync("opt/installed.json");
  }
}
let promise = dtc.say(`${isContinueInstall ? "Continue" : "Begin"} ${isBasicInstall ? "basic" : "full"} installation of NOS 2.8.7 ...`);
if (isContinueInstall) {
  promise = promise
  .then(() => dtc.exec("node", ["base-install", "continue"]));
}
else {
  promise = promise
  .then(() => dtc.exec("node", ["base-install"]));
}

promise = promise
.then(() => dtc.say(`Deadstart ${isBasicInstall ? "basic installed system" : "system to install optional products"} ...`))
.then(() => dtc.start({
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
}));

if (isBasicInstall === false) {
  const installCmd = isContinueInstall ? ["install-product", "all"] : ["install-product", "-f", "all"];
  promise = promise
  .then(() => dtc.sleep(5000))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
  .then(() => dtc.say("Begin installing optional products ..."))
  .then(() => dtc.exec("node", installCmd))
  .then(() => {
    if (fs.existsSync("opt/installed.json") === false
        || installedProductSet.length !== JSON.parse(fs.readFileSync("opt/installed.json", "utf8")).length) {
      return dtc.say("Make a new deadstart tape ...")
      .then(() => dtc.exec("node", ["make-ds-tape"]))
      .then(() => dtc.say("Shutdown system to deadstart using new tape ..."))
      .then(() => dtc.connect())
      .then(() => dtc.expect([ {re:/Operator> $/} ]))
      .then(() => dtc.shutdown(false))
      .then(() => dtc.sleep(5000))
      .then(() => dtc.say("Save previous deadstart tape and rename new one ..."))
      .then(() => {
        if (fs.existsSync("tapes/ods.tap")) {
          fs.unlinkSync("tapes/ods.tap");
        }
        fs.renameSync("tapes/ds.tap", "tapes/ods.tap");
        fs.renameSync("tapes/newds.tap", "tapes/ds.tap");
      })
      .then(() => dtc.start({
        detached: true,
        stdio:    [0, "ignore", 2]
      }));
    }
    else {
      return Promise.resolve();
    }
  })
}
promise = promise
.then(() => dtc.say(`${isBasicInstall ? "Basic" : "Full"} installation of NOS 2.8.7 complete`))
.then(() => dtc.say("Use 'node shutdown' to shutdown gracefully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
