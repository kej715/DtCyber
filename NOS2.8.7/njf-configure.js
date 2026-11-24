#!/usr/bin/env node
//
// Create the NJF configuration including:
//
//  1. The source for the NJF host configuration file
//  2. PID/LID definitions for NJE nodes with LIDs defined
//
// After creating the configuration files:
//  1. The host configuration file is compiled, and the
//     resulting HCFFILE is moved to SYSTEMX.
//  2. The LIDCMid file is updated to add PIDs/LIDs of
//     nodes with LIDs
//

const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

if (!utilities.isInstalled("njf")) {
  process.stdout.write("NJF is not installed\n");
  process.exit(0);
}

dtc.say("Start NJF configuration ...")
.then(() => dtc.exec("node", ["opt/njf-generate-hcf"]))
.then(() => dtc.exec("node", ["opt/njf-compile-hcf"]))
.then(() => {
  if (process.argv.indexOf("-l") < 0) {
    return dtc.exec("node", ["lid-configure"]);
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.say("NJF configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
