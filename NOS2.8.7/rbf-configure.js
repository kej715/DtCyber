#!/usr/bin/env node
//
// Create the RBF configuration including:
//
//  1. User definitions associated with RNF terminals
//

const fs        = require("fs");
const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

if (!utilities.isInstalled("rbf5")) {
  process.stdout.write("RBF is not installed\n");
  process.exit(0);
}

dtc.say("Start RBF configuration ...")
.then(() => dtc.exec("node", ["update-nhp-users"]))
.then(() => dtc.say("RBF configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
