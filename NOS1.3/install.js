#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");

const dtc = new DtCyber();

let isBasicInstall      = false;
let isReadyToRunInstall = true;

for (let arg of process.argv.slice(2)) {
  arg = arg.toLowerCase();
  if (arg === "basic") {
    isBasicInstall      = true;
    isReadyToRunInstall = false;
  }
  else if (arg === "full") {
    isBasicInstall      = false;
    isReadyToRunInstall = false;
  }
  else if (arg === "readytorun") {
    isBasicInstall      = false;
    isReadyToRunInstall = true;
  }
  else {
    process.stderr.write(`Unrecognized argument: ${arg}\n`);
    process.stderr.write("Usage: node install [basic | full | readytorun]\n");
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
  .then(() => dtc.attachPrinter("LP5xx_C11_E5"))
  .then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
  .then(() => dtc.say("Deadstart complete"));
};

let maxProgressLen = 0;

//
// If a ready-to-run installation is requested, execute the ready-to-run installation process,
// and then exit.
//
if (isReadyToRunInstall) {
  if (fs.existsSync("./nos13rtr.zip")) {
    fs.unlinkSync("./nos13rtr.zip");
  }
  dtc.say("Download NOS 1.3 ready-to-run package ...")
  .then(() => dtc.wget("https://www.dropbox.com/s/v4ves0gqdubdni2/nos13rtr.zip?dl=1", ".", (byteCount, contentLength) => {
    maxProgressLen = reportProgress(byteCount, contentLength, maxProgressLen);
  }))
  .then(() => clearProgress(maxProgressLen))
  .then(() => dtc.say("Expand NOS 1.3 ready-to-run package ..."))
  .then(() => dtc.unzip("./nos13rtr.zip", "."))
  .then(() => {
    fs.unlinkSync("./nos13rtr.zip");
    return Promise.resolve();
  })
  .then(() => dtc.say("Installation of ready-to-run NOS 1.3 system complete"))
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
      .then(() => dtc.shutdown(false))
      .then(() => dtc.sleep(5000))
      .then(() => startSystem());
    }
    else {
      return Promise.resolve();
    }
  })
  .then(() => dtc.say("Enter 'exit' command to exit and shutdown gracefully"))
  .then(() => dtc.engageOperator(cmdExtensions))
  .then(() => dtc.shutdown())
  .then(() => {
    process.exit(0);
  })
  .catch(err => {
    console.log(err);
    process.exit(1);
  });
}
//
// Either a basic or full install from scratch has been requested.
// Empty the tape image cache in case a download failure occurred during
// a previous run and one or more tape images are incomplete.
//
else {

  let promise = dtc.say("Begin installation of NOS 1.3 ...");
  for (const baseTape of ["ds.tap", "opl485.tap"]) {
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
  .then(() => dtc.expect([{ re: /Operator> $/ }]))
  .then(() => dtc.console("idle off"))
  .then(() => dtc.attachPrinter("LP5xx_C11_E5"))
  .then(() => dtc.dsd([
    "",
    "]!",
    "INITIALIZE,1,AL.",
    "INITIALIZE,2,AL.",
    "INITIALIZE,3,AL.",
    "INITIALIZE,4,AL.",
    "INITIALIZE,11,AL.",
    "INITIALIZE,12,AL.",
    "INITIALIZE,13,AL.",
    "INITIALIZE,14,AL.",
    "INITIALIZE,17,AL.",
    "GO.",
    "#1500#%year%%mon%%day%",
    "#1500#%hour%%min%%sec%"
  ]))
  .then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
  .then(() => dtc.say("Initial deadstart complete"))
  .then(() => dtc.say("Create VALIDUZ in default family ..."))
  .then(() => dtc.dis([
    "MODVAL,OP=C.",
    "DEFINE,VI=VALINDZ,VU=VALIDUZ.",
    "REWIND,NEWVAL,VALINDZ.",
    "COPY,NEWVAL,VU.",
    "COPY,VALINDZ,VI.",
    "CLEAR.",
    "ISF."
  ]))
  .then(() => dtc.say("Create user INSTALL ..."))
  .then(() => dtc.dis([
    "MODVAL,OP=Z./INSTALL,PW=INSTALL,FUI=1,AW=ALL,MT=7",
    "MODVAL,OP=Z./INSTALL,RP=7,SC=77B,CM=77B,EC=77B",
    "MODVAL,OP=Z./INSTALL,OF=7,DB=7,DS=7,FS=7,FC=7",
    "MODVAL,OP=Z./INSTALL,CS=7,CP=77B,CC=77B,DF=77B"
  ]))
  .then(() => dtc.say("Apply mods to OPL485, and copy it to INSTALL catalog ..."))
  .then(() => dtc.mount(13, 0, 1, "tapes/opl485.tap"))
  .then(() => dtc.runJob(11, 4, "decks/create-opl485.job", [51]))
  .then(() => dtc.say("Make a copy of SYSTEM to support customization ..."))
  .then(() => dtc.dis([
    "COMMON,SYSTEM.",
    "DEFINE,SYS=SYSTEM.",
    "COPY,SYSTEM,SYS."
  ], 1))
  .then(() => dtc.say("Create user GUEST ..."))
  .then(() => dtc.dis("MODVAL,OP=Z./GUEST,PW=GUEST,FUI=32B,AW=CCNR,SC=77B"))
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
      .then(() => dtc.expect([{ re: /Operator> $/ }]));
    }
    else {
      return Promise.resolve();
    }
  });
  //
  // Install PLATO if a full installation is requested
  //
  if (isBasicInstall === false) {
    promise = promise
    .then(() => dtc.say("Create VALIDUZ in family PLATO ..."))
    .then(() => dtc.dis([
      "FAMILY,PLATO.",
      "MODVAL,FM=PLATO,OP=C.",
      "DEFINE,VI=VALINDZ,VU=VALIDUZ.",
      "REWIND,NEWVAL,VALINDZ.",
      "COPY,NEWVAL,VU.",
      "COPY,VALINDZ,VI.",
      "CLEAR.",
      "ISF."
    ]))
    .then(() => dtc.say("Create user PLATO in family PLATO ..."))
    .then(() => dtc.dis([
      "FAMILY,PLATO.",
      "MODVAL,FM=PLATO,OP=Z./PLATO,PW=PLATO,FUI=1,AW=ALL",
      "MODVAL,FM=PLATO,OP=Z./PLATO,MT=7,RP=7,SC=77B",
      "MODVAL,FM=PLATO,OP=Z./PLATO,CM=77B,EC=77B,OF=7",
      "MODVAL,FM=PLATO,OP=Z./PLATO,DB=7,DS=7,FS=7,FC=7",
      "MODVAL,FM=PLATO,OP=Z./PLATO,CS=7,CP=77B,CC=77B",
      "MODVAL,FM=PLATO,OP=Z./PLATO,DF=77B"
    ]))
    .then(() => dtc.say("Get PLATO PFDUMP tape ..."))
    .then(() => dtc.wget("https://www.dropbox.com/s/q8v19dxqjfr7j2w/plato4nos13.tap.bz2?dl=1", "tapes", (byteCount, contentLength) => {
      maxProgressLen = reportProgress(byteCount, contentLength, maxProgressLen);
    }))
    .then(() => clearProgress(maxProgressLen))
    .then(() => dtc.say("Decompress PLATO PFDUMP tape ..."))
    .then(() => dtc.bunzip2("tapes/plato4nos13.tap.bz2", "tapes/plato4nos13.tap"))
    .then(() => dtc.say("Load PLATO PFDUMP tape on tape drive ..."))
    .then(() => dtc.dsd([
      "[UNLOAD,51.",
      "[!"
    ]))
    .then(() => dtc.sleep(2000))
    .then(() => dtc.mount(13, 0, 1, "tapes/plato4nos13.tap"))
    .then(() => dtc.say("Create job to restore PLATO master files ..."))
    .then(() => dtc.runJob(11, 4, "decks/upload-platomf-pfload.job"))
    .then(() => dtc.say("Run job to restore PLATO master files ..."))
    .then(() => dtc.dis([
      "GET,PFLPLMF.",
      "ROUTE,PFLPLMF,DC=IN,OT=SYOT."
    ], 1))
    .then(() => dtc.waitJob("PFLPLMF"))
    .then(() => dtc.say("Get PLATO development tools PFDUMP tape ..."))
    .then(() => {
      maxProgressLen = 0;
      return Promise.resolve();
    })
    .then(() => dtc.wget("https://www.dropbox.com/s/0dyv29cmf4rse9t/platodv4nos13.tap.bz2?dl=1", "tapes", (byteCount, contentLength) => {
      maxProgressLen = reportProgress(byteCount, contentLength, maxProgressLen);
    }))
    .then(() => clearProgress(maxProgressLen))
    .then(() => dtc.say("Decompress PLATO development tools PFDUMP tape ..."))
    .then(() => dtc.bunzip2("tapes/platodv4nos13.tap.bz2", "tapes/platodv4nos13.tap"))
    .then(() => dtc.say("Load PLATO development tools PFDUMP tape on tape drive ..."))
    .then(() => dtc.dsd([
      "[UNLOAD,51.",
      "[!"
    ]))
    .then(() => dtc.sleep(2000))
    .then(() => dtc.mount(13, 0, 1, "tapes/platodv4nos13.tap"))
    .then(() => dtc.say("Create job to restore PLATO development tools ..."))
    .then(() => dtc.runJob(11, 4, "decks/upload-platodv-pfload.job"))
    .then(() => dtc.say("Run job to restore PLATO development tools ..."))
    .then(() => dtc.dis([
      "GET,PFLPLDV.",
      "ROUTE,PFLPLDV,DC=IN,OT=SYOT."
    ], 1))
    .then(() => dtc.waitJob("PFLPLDV"))
    .then(() => dtc.sleep(4000));
  }

  promise = promise
  .then(() => dtc.say("Shutdown and then deadstart freshly installed system ..."))
  .then(() => dtc.shutdown(false))
  .then(() => dtc.sleep(5000))
  .then(() => startSystem())
  .then(() => dtc.say(`${isBasicInstall ? "Basic" : "Full"} installation of NOS 1.3 complete`))
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
}
