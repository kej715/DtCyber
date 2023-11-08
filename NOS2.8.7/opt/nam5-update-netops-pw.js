#!/usr/bin/env node

const DtCyber   = require("../../automation/DtCyber");
const utilities = require("./utilities");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("../LP5xx_C12_E5"))
.then(() => dtc.say("Idle NAM ..."))
.then(() => dtc.dsd("[IDLE,NAM."))
.then(() => dtc.sleep(15000))
.then(() => dtc.say("Start NAMNOGO to set NETOPS password ..."))
.then(() => dtc.dsd("NANOGO."))
.then(() => dtc.sleep(5000))
.then(() => dtc.dsd(`[CFO,NAM.UN=NETOPS,PW=${utilities.getPropertyValue(utilities.getCustomProperties(dtc),"PASSWORDS","NETOPS","NETOPSX")}`))
.then(() => dtc.sleep(1000))
.then(() => dtc.say("Restart NAM ..."))
.then(() => dtc.dsd("[CFO,NAM.GO"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(`Error caught: ${err}`);
  process.exit(1);
});
