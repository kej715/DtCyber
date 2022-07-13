#!/usr/bin/env node

const fs = require("fs");
const DtCyber = require("../automation/DtCyber");

let isContinueInstall = false;

for (let arg of process.argv.slice(2)) {
  arg = arg.toLowerCase();
  if (arg === "cont" || arg === "continue") {
    isContinueInstall = true;
  }
  else {
    process.stderr.write(`Unrecognized argument: ${arg}\n`);
    process.stderr.write("Usage: node base-install [continue]\n");
    process.exit(1);
  }
}

let completedSteps = [];
if (isContinueInstall && fs.existsSync("opt/completed-base-steps.json")) {
  completedSteps = JSON.parse(fs.readFileSync("opt/completed-base-steps.json"));
}

const addCompletedStep = step => {
  if (isCompletedStep(step) === false) {
    completedSteps.push(step);
    fs.writeFileSync("opt/completed-base-steps.json", JSON.stringify(completedSteps));
  }
};

const isCompletedStep = step => {
  return completedSteps.indexOf(step) !== -1;
};

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

if (isCompletedStep("init")) {
  promise = promise
  .then(() => dtc.start(dtCyberPath, []))
  .then(() => dtc.expect([ {re:/Operator> $/} ]))
  .then(() => dtc.say("DtCyber started using default profile"))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
  .then(() => dtc.say("Deadstart complete"));
}
else {
  promise = promise
  .then(() => dtc.start(dtCyberPath, ["manual"]))
  .then(() => dtc.expect([ {re:/Operator> $/} ]))
  .then(() => dtc.say("DtCyber started using manual profile"))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.say("Begin initial deadstart ..."))
  .then(() => dtc.dsd([
    "O!",
    "#500#P!",
    "#500#D=YES",
    "#500#",
    "#4500#NEXT.",
    "#1000#]!",
    "#1000#INITIALIZE,AL,5,6,10,11,12,13.",
    "#500#GO.",
    "#1500#%year%%mon%%day%",
    "#1500#%hour%%min%%sec%"
  ]))
  .then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
  .then(() => {
    addCompletedStep("init");
    return Promise.resolve();
  })
  .then(() => dtc.say("Initial deadstart complete"));
}

let isMountTapes = false;

if (isCompletedStep("sysgen-full") === false) {
  isMountTapes = true;
  promise = promise
  .then(() => dtc.say("Start SYSGEN(FULL) ..."))
  .then(() => dtc.mount(13, 0, 1, "tapes/nos287-1.tap"))
  .then(() => dtc.mount(13, 0, 2, "tapes/nos287-2.tap"))
  .then(() => dtc.mount(13, 0, 3, "tapes/nos287-3.tap"))
  .then(() => dtc.dsd("X.SYSGEN(FULL)"))
  .then(() => dtc.expect([ {re:/E N D   F U L L/} ], "printer"))
  .then(() => dtc.say("SYSGEN(FULL) complete"))
  .then(() => {
    addCompletedStep("sysgen-full");
    return Promise.resolve();
  });
}

if (isCompletedStep("nam-init") === false) {
  promise = promise
  .then(() => dtc.say("Create and compile the NDL file ..."))
  .then(() => dtc.runJob(12, 4, "decks/create-ndlopl.job"))
  .then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job"))
  .then(() => dtc.say("Update the NAMSTRT file ..."))
  .then(() => dtc.dis("PERMIT,NAMSTRT,INSTALL=W.", 377772))
  .then(() => dtc.runJob(12, 4, "decks/update-namstrt.job"))
  .then(() => dtc.say("Create the TCPHOST file ..."))
  .then(() => dtc.runJob(12, 4, "decks/create-tcphost.job"))
  .then(() => {
    addCompletedStep("nam-init");
    return Promise.resolve();
  });
}

if (isCompletedStep("termlib-update") === false) {
  promise = promise
  .then(() => dtc.say("Add terminal definitions to TERMLIB ..."))
  .then(() => dtc.dis("PERMIT,TERMLIB,INSTALL=W.", 377776))
  .then(() => dtc.runJob(12, 4, "decks/update-termlib.job"))
  .then(() => {
    addCompletedStep("termlib-update");
    return Promise.resolve();
  });
}

if (isCompletedStep("add-guest") === false) {
  promise = promise
  .then(() => dtc.say("Add a non-privileged GUEST user ..."))
  .then(() => dtc.dsd("X.MS(VALUSER,LIMITED,GUEST,GUEST)"))
  .then(() => {
    addCompletedStep("add-guest");
    return Promise.resolve();
  });
}

if (isCompletedStep("sysgen-source") === false) {
  promise = promise
  .then(() => dtc.say("Start SYSGEN(SOURCE) ..."))
  .then(() => dtc.dsd("[X.SYSGEN(SOURCE)"))
  .then(() => dtc.expect([ {re:/E N D   S O U R C E/} ], "printer"))
  .then(() => dtc.say("SYSGEN(SOURCE) complete"))
  .then(() => {
    addCompletedStep("sysgen-source");
    return Promise.resolve();
  });
}

if (isCompletedStep("prep-customization") === false) {
  promise = promise
  .then(() => dtc.say("Run job to initialize customization artifacts ..."))
  .then(() => dtc.runJob(12, 4, "decks/init-build.job"))
  .then(() => dtc.say("Run job to load and update OPL871 ..."))
  .then(() => dtc.runJob(12, 4, "decks/modopl.job"))
  .then(() => {
    addCompletedStep("prep-customization");
    return Promise.resolve();
  });
}

if (isMountTapes) {
  promise = promise
  .then(() => dtc.say("Unload system source tapes ..."))
  .then(() => dtc.dsd([
    "[UNLOAD,51.",
    "[UNLOAD,52.",
    "[UNLOAD,53."
  ]));
}

promise = promise
.then(() => dtc.say("Base installation completed successfully"))
.then(() => dtc.shutdown())
.catch(err => {
  console.log(`Error caught: ${err}`);
  process.exit(1);
});
