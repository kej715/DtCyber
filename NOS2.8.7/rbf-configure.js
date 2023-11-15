#!/usr/bin/env node
//
// Create the RBF configuration including:
//
//  1. NDL modset defining additional terminals used by RBF
//  2. User definitions associated with additional terminals
//  3. Update of cyber.ovl file to include terminal
//     definitions for DtCyber
//

const fs        = require("fs");
const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const customProps  = utilities.getCustomProperties(dtc);
const terminals    = utilities.getHaspTerminals(dtc);
const names        = Object.keys(terminals);

if (names.length < 1) process.exit(0);

dtc.say("Start RBF configuration ...")
.then(() => {
  if (process.argv.indexOf("-ndl") === -1) {
    return dtc.exec("node", ["opt/rbf5-update-ndl"])
    .then(() => dtc.exec("node", ["compile-ndl"]));
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Create users for RBF terminals ..."))
.then(() => {
  let job = [
    "COPYBR,INPUT,RBFUSRS.",
    "REPLACE,RBFUSRS."
  ];
  let userDefns = [];
  for (const name of names) {
    let pw = (name.length < 4) ? `${name}X` : name;
    userDefns.push(`/${name},PW=${utilities.getPropertyValue(customProps, "PASSWORDS", name, pw)},RL=ALL,AP=NUL,AP=RBF`);
  }
  const options = {
    jobname: "RBFUSRS",
    username: "INSTALL",
    password: utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL"),
    data: userDefns.join(",\n") + "\n"
  };
  return dtc.createJobWithOutput(12, 4, job, options)
  .then(output => dtc.dis([
    "GET,RBFUSRS.",
    "PURGE,RBFUSRS.",
    "MODVAL,FA,I=RBFUSRS,OP=U."
  ], "RBFMODV", 1));
})
.then(() => dtc.exec("node", ["opt/rbf5-update-ovl"]))
.then(() => dtc.say("RBF configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
