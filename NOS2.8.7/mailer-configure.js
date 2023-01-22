#!/usr/bin/env node
//
// Update the MAILER site configuration record to set the
// local host network identifier and time zone.
//

const fs        = require("fs");
const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const hostId   = utilities.getHostId(dtc);
const timeZone = utilities.getTimeZone();

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Upload and run procedure to configure MAILER database"))
.then(() => dtc.say(`  Host ID is ${hostId.toLowerCase()}, time zone is ${timeZone}`))
.then(() => dtc.runJob(12, 4, "opt/mailer-cfg.job", [hostId, timeZone]))
.then(() => dtc.dis([
  "GET,MLRCFG.",
  "PURGE,MLRCFG.",
  "SUI,377777.",
  "MLRCFG."
], 1))
.then(() => dtc.say("MAILER configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
