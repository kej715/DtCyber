#!/usr/bin/env node
//
// Create the RHP configuration including:
//
//  1. NDL modset defining trunks and INCALL/OUTCALL definitions
//     used by RHP
//  2. PID/LID definitions for RHP nodes
//  3. Update of cyber.ovl file to include trunk definitions
//     for links to RHP nodes
//
// After creating the configuration files:
//  1. The NDL modset is applied and the NDL configuration
//     file is compiled
//  2. The LIDCMid file is updated to add PIDs/LIDs of
//     RHP nodes
//

const DtCyber   = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.say("Start RHP configuration ...")
.then(() => {
  if (process.argv.indexOf("-ndl") === -1) {
    return dtc.exec("node", ["opt/rhp-update-ndl"])
    .then(() => dtc.exec("node", ["compile-ndl"]));
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.exec("node", ["opt/rhp-update-eqpd"]))
.then(() => dtc.exec("node", ["opt/rhp-update-ovl"]))
.then(() => dtc.exec("node", ["lid-configure"]))
.then(() => dtc.say("RHP configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
