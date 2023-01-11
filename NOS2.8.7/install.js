#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");
const utilities     = require("./opt/utilities");

const dtc = new DtCyber();

let isBasicInstall      = false;
let isContinueInstall   = false;
let isReadyToRunInstall = true;

for (let arg of process.argv.slice(2)) {
  arg = arg.toLowerCase();
  if (arg === "basic") {
    isBasicInstall      = true;
    isReadyToRunInstall = false;
  }
  else if (arg === "cont" || arg === "continue") {
    isContinueInstall = true;
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
    process.stderr.write("Usage: node install [basic | full | readytorun][continue]\n");
    process.exit(1);
  }
}

/*
 * startSystem
 *
 * Utility function that starts DtCyber as a background process and returns a promise
 * that is fulfilled when deadstart of the operating system has completed.
 */
const startSystem = () => {
  return dtc.start({
    detached: true,
    stdio:    [0, "ignore", 2],
    unref:    false
  })
  .then(() => dtc.sleep(5000))
  .then(() => dtc.connect())
  .then(() => dtc.expect([{ re: /Operator> $/ }]))
  .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
  .then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
  .then(() => dtc.say("Deadstart complete"));
};

//
// If a ready-to-run installation is requested, execute the ready-to-run installation process,
// and then exit.
//
if (isReadyToRunInstall) {
  let maxProgressLen = 0;
  if (fs.existsSync("./nos287rtr.zip")) {
    fs.unlinkSync("./nos287rtr.zip");
  }
  dtc.say("Download NOS 2.8.7 ready-to-run package ...")
  .then(() => dtc.wget("https://www.dropbox.com/s/88ozv7u1qlpc27j/nos287rtr.zip?dl=1", ".", (byteCount, contentLength) => {
    maxProgressLen = utilities.reportProgress(byteCount, contentLength, maxProgressLen);
  }))
  .then(() => utilities.clearProgress(maxProgressLen))
  .then(() => dtc.say("Expand NOS 2.8.7 ready-to-run package ..."))
  .then(() => dtc.unzip("./nos287rtr.zip", "."))
  .then(() => {
    fs.unlinkSync("./nos287rtr.zip");
    return Promise.resolve();
  })
  .then(() => dtc.say("Installation of ready-to-run NOS 2.8.7 system complete"))
  .then(() => dtc.say("Deadstart ready-to-run system"))
  .then(() => startSystem())
  .then(() => {
    //
    //  If the file site.cfg exists, run the reconfiguration tool to
    //  apply configuration customizations to the installed system.
    //
    if (fs.existsSync("site.cfg")) {
      return dtc.disconnect()
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
  dtc.say(`${isContinueInstall ? "Continue" : "Begin"} ${isBasicInstall ? "basic" : "full"} installation of NOS 2.8.7 ...`)
  .then(() => {
    return isContinueInstall ? dtc.exec("node", ["base-install", "continue"]) : dtc.exec("node", ["base-install"]);
  })
  .then(() => dtc.say(`Deadstart ${isBasicInstall ? "basic installed system" : "system to install optional products"} ...`))
  .then(() => startSystem())
  .then(() => {
    if (isBasicInstall) {
      return Promise.resolve();
    }
    else {
      const installCmd = isContinueInstall ? ["install-product", "all"] : ["install-product", "-f", "all"];
      return dtc.say("Begin installing optional products ...")
      .then(() => dtc.disconnect())
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
          .then(() => {
            if (fs.existsSync("tapes/ods.tap")) {
              fs.unlinkSync("tapes/ods.tap");
            }
            fs.renameSync("tapes/ds.tap", "tapes/ods.tap");
            fs.renameSync("tapes/newds.tap", "tapes/ds.tap");
            return Promise.resolve();
          })
          .then(() => dtc.say("Deadstart system using new tape"))
          .then(() => startSystem());
        }
        else {
          return Promise.resolve();
        }
      });
    }
  })
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

  const waitForExplicitExit = () => {
    setTimeout(waitForExplicitExit, 60000);
  }
  waitForExplicitExit();
}
