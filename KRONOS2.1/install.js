#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");

const dtc = new DtCyber();

let isReadyToRunInstall = true;

for (let arg of process.argv.slice(2)) {
  arg = arg.toLowerCase();
  if (arg === "full") {
    isReadyToRunInstall = false;
  }
  else if (arg === "readytorun") {
    isReadyToRunInstall = true;
  }
  else {
    process.stderr.write(`Unrecognized argument: ${arg}\n`);
    process.stderr.write("Usage: node install [full | readytorun]\n");
    process.exit(1);
  }
}

const clearProgress = maxProgressLen => {
  let progress = `\r`;
  while (progress.length++ < maxProgressLen) progress += " ";
  process.stdout.write(`${progress}\r`);
  return Promise.resolve();
};

const reportProgress = (byteCount, contentLength, maxProgressLen) => {
  let progress = `\r${new Date().toLocaleTimeString()}   Received ${byteCount}`;
  if (contentLength === -1) {
    progress += " bytes";
  }
  else {
    progress += ` of ${contentLength} bytes (${Math.round((byteCount / contentLength) * 100)}%)`;
  }
  process.stdout.write(progress)
  return (progress.length > maxProgressLen) ? progress.length : maxProgressLen;
};

/*
 * shutdown
 *
 * Utility function that shuts down DtCyber gracefully and returns a promise
 * that is fulfilled when the shutdown is complete.
 *
 * Arguments:
 *   args - optional array of command line arguments to be passed to DtCyber
 */
const shutdown = () => {
  dtc.setExitOnClose(false);
  return dtc.say("Starting shutdown sequence ...")
  .then(() => dtc.dsd("[UNLOCK."))
  .then(() => dtc.dsd("[BLITZ."))
  .then(() => dtc.sleep(5000))
  .then(() => dtc.dsd("CHECK#2000#"))
  .then(() => dtc.sleep(2000))
  .then(() => dtc.dsd("STEP."))
  .then(() => dtc.sleep(1000))
  .then(() => dtc.send("shutdown"))
  .then(() => dtc.expect([{ re: /Goodbye for now/ }]))
  .then(() => dtc.say("Shutdown complete ..."));
};

/*
 * startSystem
 *
 * Utility function that starts DtCyber as a background process and returns a promise
 * that is fulfilled when deadstart of the operating system has completed.
 *
 * Arguments:
 *   args - optional array of command line arguments to be passed to DtCyber
 */
const startSystem = args => {
  return dtc.start(args, {
    detached: true,
    stdio:    [0, "ignore", 2],
    unref:    false
  })
  .then(() => dtc.sleep(5000))
  .then(() => dtc.connect())
  .then(() => dtc.expect([{ re: /Operator> $/ }]))
  .then(() => dtc.attachPrinter("LP5xx_C12_E6"))
  .then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
  .then(() => dtc.say("Deadstart complete"));
};

let maxProgressLen = 0;
let promise        = Promise.resolve();

//
// If a ready-to-run installation is requested, execute the ready-to-run installation process,
// and then exit.
//
if (isReadyToRunInstall) {
  if (fs.existsSync("./kronos21rtr.zip")) {
    fs.unlinkSync("./kronos21rtr.zip");
  }
  promise = promise
  .then(() => dtc.say("Download KRONOS 2.1 ready-to-run package ..."))
  .then(() => dtc.wget("https://www.dropbox.com/s/cmr9eh8sfdu4lhi/kronos21rtr.zip?dl=1", ".", (byteCount, contentLength) => {
    maxProgressLen = reportProgress(byteCount, contentLength, maxProgressLen);
  }))
  .then(() => clearProgress(maxProgressLen))
  .then(() => dtc.say("Expand KRONOS 2.1 ready-to-run package ..."))
  .then(() => dtc.unzip("./kronos21rtr.zip", "."))
  .then(() => {
    fs.unlinkSync("./kronos21rtr.zip");
    return Promise.resolve();
  })
  .then(() => dtc.say("Installation of ready-to-run KRONOS 2.1 system complete"))
  .then(() => dtc.say("Deadstart ready-to-run system"))
  .then(() => startSystem())
  .then(() => {
    //
    //  If the file site.cfg exists, run the reconfiguration tool to
    //  apply configuration customizations to the installed system.
    //
    if (fs.existsSync("site.cfg")) {
      return dtc.unmount(13, 0, 0)
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
      .then(() => dtc.connect())
      .then(() => dtc.expect([{ re: /Operator> $/ }]))
      .then(() => dtc.say("Shutdown and re-deadstart using the new tape"))
      .then(() => shutdown())
      .then(() => dtc.sleep(5000))
      .then(() => startSystem());
    }
    else {
      return Promise.resolve();
    }
  });
}
//
// A full install from scratch has been requested.
//
else {

  promise = promise
  .then(() => dtc.say("Begin installation of KRONOS 2.1 ..."));
  for (const baseTape of ["ds.tap", "opl404.tap"]) {
    if (!fs.existsSync(`tapes/${baseTape}`)) {
      promise = promise
      .then(() => dtc.say(`Decompress tapes/${baseTape}.bz2 to tapes/${baseTape} ...`))
      .then(() => dtc.bunzip2(`tapes/${baseTape}.bz2`, `tapes/${baseTape}`));
    }
  }
  
  promise = promise
  .then(() => dtc.start(["manual"], {
    detached: true,
    stdio:    [0, "ignore", 2],
    unref:    false
  }))
  .then(() => dtc.sleep(5000))
  .then(() => dtc.connect())
  .then(() => dtc.expect([ {re:/Operator> $/} ]))
  .then(() => dtc.console("idle off"))
  .then(() => dtc.say("DtCyber started"))
  .then(() => dtc.attachPrinter("LP5xx_C12_E6"))
  .then(() => dtc.dsd([
    "",
    "]!",
    "INITIALIZE,0,AL.",
    "INITIALIZE,1,AL.",
    "GO.",
    "#1500#%year%%mon%%day%",
    "#1500#%hour%%min%%sec%"
  ]))
  .then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
  .then(() => dtc.say("Initial deadstart complete"))
  .then(() => dtc.say("Create VALIDUS with INSTALL user in default family ..."))
  .then(() => dtc.dis([
    "MODVAL,OP=C.",
    "DEFINE,VI=VALINDS,VU=VALIDUS.",
    "REWIND,NEWVAL,VALINDS.",
    "COPY,NEWVAL,VU.",
    "COPY,VALINDS,VI.",
    "CLEAR.",
    "ISF.",
    "MODVAL,OP=Z./INSTALL,PW=INSTALL,FUI=1,AW=ALL,DB=1"
  ]))
  .then(() => dtc.say("Update INSTALL user and create GUEST user ..."))
  .then(() => dtc.runJob(12, 4, "decks/create-users.job"))
  .then(() => dtc.dis([
    "GET,USERS.",
    "MODVAL,FA,I=USERS,OP=U.",
    "PURGE,USERS/NA."
  ], 1))
  .then(() => dtc.sleep(1000))
  .then(() => dtc.say("Apply mods to OPL404, and copy it to INSTALL catalog ..."))
  .then(() => dtc.mount(13, 0, 1, "tapes/opl404.tap"))
  .then(() => dtc.runJob(12, 4, "decks/create-opl404.job", [51]))
  .then(() => dtc.say("Make a copy of SYSTEM to support customization ..."))
  .then(() => dtc.dis([
    "COMMON,SYSTEM.",
    "DEFINE,SYS=SYSTEM.",
    "COPY,SYSTEM,SYS."
  ], 1))
  .then(() => dtc.sleep(5000))
  .then(() => {
    //
    //  If the file site.cfg exists, run the reconfiguration tool to
    //  apply configuration customizations to the installed system.
    //
    if (fs.existsSync("site.cfg")) {
      return dtc.unmount(13, 0, 0)
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
      .then(() => dtc.connect())
      .then(() => dtc.expect([{ re: /Operator> $/ }]))
      .then(() => dtc.say("Shutdown and re-deadstart using the new tape"))
      .then(() => shutdown())
      .then(() => dtc.sleep(5000))
      .then(() => startSystem());
    }
    else {
      return Promise.resolve();
    }
  })
  .then(() => dtc.say("Full installation of KRONOS 2.1 complete"));
}

promise = promise
.then(() => dtc.say("Enter 'exit' command to exit and shutdown gracefully"))
.then(() => dtc.engageOperator(cmdExtensions))
.then(() => shutdown())
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

