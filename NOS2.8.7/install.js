#!/usr/bin/env node

const fs = require("fs");
const DtCyber = require("../automation/DtCyber");

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

for (const name of fs.readdirSync("opt/tapes")) {
  if (name.toLowerCase() !== "dummy") fs.unlinkSync(`opt/tapes/${name}`);
}

const dtc = new DtCyber();

let promise = dtc.say(`${isContinueInstall ? "Continue" : "Begin"} ${isBasicInstall ? "basic" : "full"} installation of NOS 2.8.7 ...`);
if (isContinueInstall) {
  promise = promise
  .then(() => dtc.exec("node", ["base-install", "continue"]));
}
else {
  fs.writeFileSync("opt/installed.json", JSON.stringify(["atf","iaf","nam5","nos","tcph"]));
  promise = promise
  .then(() => dtc.exec("node", ["base-install"]));
}

if (isBasicInstall) {
  promise = promise
  .then(() => dtc.say("Re-deadstart basic installed system ..."));
}
else {
  promise = promise
  .then(() => dtc.say("Re-deadstart system to begin installing products ..."))
  .then(() => dtc.exec(dtCyberPath, [], {
    detached: true,
    shell:    true,
    stdio:    ["pipe", "ignore", 2],
    unref:    false
  }))
  .then(() => dtc.sleep(5000))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
  .then(() => dtc.say("Begin installing optional products ..."))
  .then(() => dtc.exec("node", ["install-product", "all"]))
  .then(() => dtc.say("Make a new deadstart tape ..."))
  .then(() => dtc.exec("node", ["make-ds-tape"]))
  .then(() => {
    fs.renameSync("tapes/ds.tap", "tapes/ods.tap");
    fs.renameSync("tapes/newds.tap", "tapes/ds.tap");
  })
  .then(() => dtc.connect())
  .then(() => dtc.expect([ {re:/Operator> $/} ]))
  .then(() => dtc.shutdown(false))
  .then(() => dtc.sleep(5000))
  .then(() => dtc.say("Deadstart system using new deadstart tape ..."));
}
promise = promise
.then(() => dtc.exec(dtCyberPath, [], {
  detached: true,
  shell:    true,
  stdio:    [0, "ignore", 2]
}))
.then(() => dtc.say(`${isBasicInstall ? "Basic" : "Full"} installation of NOS 2.8.7 complete`))
.then(() => dtc.say("Use 'node shutdown' to shutdown gracefully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
