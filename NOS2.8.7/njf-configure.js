#!/usr/bin/env node
//
// Create the NJF configuration including:
//
//  1. NDL modset defining the terminals used by NJF
//  2. The source for the NJF host configuration file
//  3. PID/LID definitions for NJE nodes with LIDs defined
//  4. Update of cyber.ovl file to include terminal
//     definitions for DtCyber
//
// After creating the configuration files:
//  1. The NDL modset is applied and the NDL configuration
//     file is compiled
//  2. The host configuration file is compiled, and the
//     resulting HCFFILE is moved to SYSTEMX.
//  3. The LIDCMid file is updated to add PIDs/LIDs of
//     nodes with LIDs
//

const DtCyber   = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.say("Start NJF configuration ...")
.then(() => {
  if (process.argv.indexOf("-ndl") === -1) {
    return dtc.exec("node", ["opt/njf-update-ndl"])
    .then(() => dtc.exec("node", ["compile-ndl"]));
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.exec("node", ["opt/njf-generate-hcf"]))
.then(() => dtc.exec("node", ["opt/njf-compile-hcf"]))
.then(() => dtc.exec("node", ["opt/njf-update-ovl"]))
.then(() => dtc.exec("node", ["lid-configure"]))
.then(() => dtc.say("NJF configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
