#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");

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
if (fs.existsSync("opt/installed.json") && isContinueInstall) {
  installedProductSet = JSON.parse(fs.readFileSync("opt/installed.json", "utf8"));
}
else {
  fs.writeFileSync("opt/installed.json", JSON.stringify(installedProductSet));
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
}))
.then(() => dtc.sleep(5000))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.say("Deadstart complete"));

if (isBasicInstall === false) {
  const installCmd = isContinueInstall ? ["install-product", "all"] : ["install-product", "-f", "all"];
  promise = promise
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
      .then(() => dtc.console("idle off"))
      .then(() => dtc.shutdown(false))
      .then(() => dtc.sleep(5000))
      .then(() => dtc.say("Save previous deadstart tape and rename new one ..."))
      .then(() => new Promise ((resolve, reject) => {
        if (fs.existsSync("tapes/ods.tap")) {
          fs.unlinkSync("tapes/ods.tap");
        }
        fs.renameSync("tapes/ds.tap", "tapes/ods.tap");
        fs.renameSync("tapes/newds.tap", "tapes/ds.tap");
        resolve();
      }))
      .then(() => dtc.say("Deadstart system using new tape"))
      .then(() => dtc.start({
        detached: true,
        stdio:    [0, "ignore", 2],
        unref:    false
      }))
      .then(() => dtc.sleep(5000))
      .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
      .then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
      .then(() => dtc.say("Deadstart complete"));
    }
    else {
      return Promise.resolve();
    }
  });
}
promise = promise
.then(() => dtc.connect())
.then(() => dtc.expect([{ re: /Operator> $/ }]))
.then(() => dtc.console("idle on"))
.then(() => dtc.say(`${isBasicInstall ? "Basic" : "Full"} installation of NOS 2.8.7 complete`))
.then(() => dtc.say("Enter 'exit' command to exit and shutdown gracefully"))
.then(() => dtc.engageOperator(cmdExtensions))
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

//
// The following code is necessary to keep some work on the
// event queue. Otherwise, Node.js will exit prematurely
// before all promises are fulfilled. Explicit calls to
// process.exit(), above, will enable graceful exit when
// all promises are fulfilled or an error occurs.
//
const stayAlive = () => {
  setTimeout(stayAlive, 60000);
}
stayAlive();
