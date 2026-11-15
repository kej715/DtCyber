#!/usr/bin/env node
//
//  Update validation file entries for special NHP usernames.
//  Currently, this includes usernames associated with RBF
//  terminals.
//

const fs        = require("fs");
const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

if (!utilities.isInstalled("rbf5")) process.exit(0);

const customProps = utilities.getCustomProperties(dtc);
const config      = utilities.getNhpConfiguration(dtc);

let rbfUsers = [];
for (const key of Object.keys(config)) {
  if (key.startsWith("MDI") || key.startsWith("NPU")) {
    let device = config[key];
    if (typeof device.hasp !== "undefined") {
      for (const termDef of device.hasp) {
        let ei = termDef.user.lastIndexOf("MUSER=");
        if (ei >= 0) {
          rbfUsers.push(termDef.user.substring(ei + 6));
        }
      }
    }
  }
}

if (rbfUsers.length < 1) process.exit(0);

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Create users for RBF terminals ..."))
.then(() => {
  let job = [
    "COPYBR,INPUT,RBFUSRS.",
    "REPLACE,RBFUSRS."
  ];
  let userDefns = [];
  for (const user of rbfUsers) {
    let pw = (user.length < 4) ? `${user}X` : user;
    userDefns.push(`/${user},PW=${utilities.getPropertyValue(customProps, "PASSWORDS", user, pw)},RL=ALL,AP=NUL,AP=RBF`);
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
.then(() => dtc.say("Users created successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
