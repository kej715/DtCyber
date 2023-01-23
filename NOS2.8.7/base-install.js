#!/usr/bin/env node

const fs        = require("fs");
const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const baseTapes = ["ds.tap", "nos287-1.tap", "nos287-2.tap", "nos287-3.tap"];

const dtc = new DtCyber();

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

if (isContinueInstall === false) {
  for (const baseTape of baseTapes) {
    if (fs.existsSync(`tapes/${baseTape}`)) fs.unlinkSync(`tapes/${baseTape}`);
  }
}

let promise = Promise.resolve();
for (const baseTape of baseTapes) {
  if (!fs.existsSync(`tapes/${baseTape}`)) {
    promise = promise
    .then(() => dtc.say(`Decompress tapes/${baseTape}.bz2 to tapes/${baseTape} ...`))
    .then(() => dtc.bunzip2(`tapes/${baseTape}.bz2`, `tapes/${baseTape}`));
  }
}

if (isCompletedStep("init")) {
  promise = promise
  .then(() => dtc.start({
    detached: true,
    stdio:    [0, "ignore", 2],
    unref:    false
  }))
  .then(() => dtc.sleep(2000))
  .then(() => dtc.connect())
  .then(() => dtc.expect([ {re:/Operator> $/} ]))
  .then(() => dtc.console("idle off"))
  .then(() => dtc.say("DtCyber started using default profile"))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
  .then(() => dtc.say("Deadstart complete"));
}
else {
  promise = promise
  .then(() => dtc.start(["manual"], {
    detached: true,
    stdio:    [0, "ignore", 2],
    unref:    false
  }))
  .then(() => dtc.sleep(2000))
  .then(() => dtc.connect())
  .then(() => dtc.expect([ {re:/Operator> $/} ]))
  .then(() => dtc.console("idle off"))
  .then(() => dtc.say("DtCyber started using manual profile"))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.say("Begin initial deadstart ..."))
  .then(() => dtc.dsd([
    "O!",
    "#1000#P!",
    "#1000#D=YES",
    "#1000#",
    "#5000#NEXT.",
    "#1000#]!",
    "#1000#INITIALIZE,AL,5,6,10,11,12,13.",
    "#1000#GO.",
    "#5000#%year%%mon%%day%",
    "#3000#%hour%%min%%sec%"
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
  .then(() => dtc.runJob(12, 4, "decks/update-namstrt.job", utilities.getTimeZone()))
  .then(() => dtc.say("Create the TCPHOST file ..."))
  .then(() => dtc.runJob(12, 4, "decks/create-tcphost.job"))
  .then(() => dtc.say("Set NETOPS password and security count ..."))
  .then(() => dtc.dsd("X.MODVAL(OP=Z)/NETOPS,PW=NETOPSX,SC=77B"))
  .then(() => dtc.say("Start NAMNOGO to initialize NAM ..."))
  .then(() => dtc.dsd("NANOGO."))
  .then(() => dtc.sleep(5000))
  .then(() => dtc.dsd("CFO,NAM.UN=NETOPS,PW=NETOPSX"))
  .then(() => dtc.sleep(1000))
  .then(() => dtc.dsd("CFO,NAM.GO"))
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
  .then(() => dtc.runJob(12, 4, "decks/create-guest.job"))
  .then(() => dtc.dis([
    "GET,GESTUSR.",
    "PURGE,GESTUSR.",
    "MODVAL,FA,I=GESTUSR,OP=U."
  ], "GESTMDV", 1))
  .then(() => {
    addCompletedStep("add-guest");
    return Promise.resolve();
  });
}

if (isCompletedStep("sysgen-source") === false) {
  promise = promise
  .then(() => dtc.say("Start SYSGEN(SOURCE) ..."));
  if (isMountTapes === false) {
    isMountTapes = true;
    promise = promise
    .then(() => dtc.mount(13, 0, 1, "tapes/nos287-1.tap"))
    .then(() => dtc.mount(13, 0, 2, "tapes/nos287-2.tap"))
    .then(() => dtc.mount(13, 0, 3, "tapes/nos287-3.tap"));
  }
  promise = promise
  .then(() => dtc.dsd("[X.SYSGEN(SOURCE)"))
  .then(() => dtc.expect([ {re:/E N D   S O U R C E/} ], "printer"))
  .then(() => dtc.say("SYSGEN(SOURCE) complete"))
  .then(() => {
    addCompletedStep("sysgen-source");
    return Promise.resolve();
  });
}

if (isCompletedStep("prep-customization") === false) {
  if (isMountTapes === false) {
    isMountTapes = true;
    promise = promise
    .then(() => dtc.mount(13, 0, 1, "tapes/nos287-1.tap"))
    .then(() => dtc.mount(13, 0, 2, "tapes/nos287-2.tap"))
    .then(() => dtc.mount(13, 0, 3, "tapes/nos287-3.tap"));
  }
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

if (fs.existsSync("site.cfg") && isCompletedStep("site-config") === false) {
  promise = promise
  .then(() => dtc.say("Apply site configuration (site.cfg) ..."))
  .then(() => dtc.disconnect())
  .then(() => dtc.exec("node", ["reconfigure"]))
  .then(() => dtc.say("Make a new deadstart tape ..."))
  .then(() => dtc.exec("node", ["make-ds-tape"]))
  .then(() => {
    if (fs.existsSync("tapes/ods.tap")) {
      fs.unlinkSync("tapes/ods.tap");
    }
    fs.renameSync("tapes/ds.tap", "tapes/ods.tap");
    fs.renameSync("tapes/newds.tap", "tapes/ds.tap");
    return Promise.resolve();
  })
  .then(() => {
    addCompletedStep("site-config");
    return Promise.resolve();
  })
  .then(() => dtc.connect())
  .then(() => dtc.expect([ {re:/Operator> $/} ]));
}

promise = promise
.then(() => dtc.say("Base installation completed successfully"))
.then(() => dtc.shutdown())
.catch(err => {
  console.log(`Error caught: ${err}`);
  process.exit(1);
});
