#!/usr/bin/env node

const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Run job to update the MAG startup job ..."))
.then(() => dtc.runJob(12, 4, "decks/create-mag.job"))
.then(() => dtc.dis([
  "GET,MAG.",
  "SUI,377777.",
  "REPLACE,MAG.",
  "PERMIT,MAG,INSTALL=W."
], 1))
.then(() => dtc.say("Create the TMS catalog ..."))
.then(() => dtc.dis([
  "DEFINE,NEW=ZZZZZFC.",
  "TFSP,OP=I,I=0,LF,P=0."
]))
.then(() => dtc.say("Restart MAG ..."))
.then(() => dtc.dsd("IDLE,MAG."))
.then(() => dtc.sleep(1000))
.then(() => dtc.dsd("MAG."))
.then(() => dtc.say("Validate INSTALL as a TMS administrator ..."))
.then(() => dtc.dis("TFSP,OP=Z./VALIDAT=INSTALL,GO"))
.then(() => dtc.say("TMS activated"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
