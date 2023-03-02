#!/usr/bin/env node
//
// Create the TLF configuration including:
//
//  1. NDL modset defining the terminals used by TLF
//  2. The source for the TLF configuration file
//  3. PID/LID definitions for TLF nodes
//  4. Update of cyber.ovl file to include terminal
//     definitions for DtCyber
//
// After creating the configuration files:
//  1. The NDL modset is applied and the NDL configuration
//     file is compiled
//  2. The host configuration file is compiled, and the
//     resulting TCFFILE is made available to SYSTEMX.
//  3. The LIDCMid file is updated to add PIDs/LIDs of
//     nodes with LIDs
//

const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const topology = utilities.getTlfTopology(dtc);

dtc.say("Start TLF configuration ...")
.then(() => {
  if (process.argv.indexOf("-ndl") === -1) {
    return dtc.exec("node", ["opt/tlf-update-ndl"])
    .then(() => dtc.exec("node", ["compile-ndl"]));
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.exec("node", ["opt/tlf-generate-tcf"]))
.then(() => {
  if (Object.keys(topology).length > 0) {
    return dtc.exec("node", ["opt/tlf-compile-tcf"])
    .then(() => dtc.exec("node", ["opt/tlf-update-ovl"]))
    .then(() => dtc.exec("node", ["lid-configure"]));
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.say("TLF configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
