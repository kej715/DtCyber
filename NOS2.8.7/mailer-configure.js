#!/usr/bin/env node
//
// Update the MAILER configuration including:
//
//  1. Update the MAILER site configuration record to set the
//     local host network identifier
//

const fs      = require("fs");
const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

//
// If a site configuration file exists, and it has a CMRDECK
// section with an MID definition, use that definition.
// Otherwise, use the default machine ID, "01".
//
let customProps = {};
let mid = "01";
dtc.readPropertyFile(customProps);
if (typeof customProps["CMRDECK"] !== "undefined") {
  for (let line of customProps["CMRDECK"]) {
    line = line.toUpperCase();
    let match = line.match(/^MID=([^.]*)/);
    if (match !== null) {
      mid = match[1].trim();
    }
  }
}

//
// Discover the MAILER host network identifier. Preset it to the machine ID or
// the configured hostID. It likely will be overridden by the primary host name
// defined in the TCPHOST file.
//
let mailerHostId = `M${mid}`;
if (typeof customProps["NETWORK"] !== "undefined") {
  for (let line of customProps["NETWORK"]) {
    line = line.toUpperCase();
    let match = line.match(/^HOSTID=([^.]*)/);
    if (match !== null) {
      mailerHostId = match[1].trim();
    }
  }
}

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Discover host network ID"))
.then(() => dtc.createJobWithOutput(12, 4, [
  `$GET,TCPHOST/UN=NETADMN.`,
  "$COPYSBF,TCPHOST."
], {jobname:"GETHOST"}))
.then(text => {
  //
  // Find host domain name in TCPHOST or [HOSTS] section of custom props
  //
  text = dtc.cdcToAscii(text);
  const hostIdPattern = new RegExp(`LOCALHOST_${mid}`, "i");
  for (let line of text.split("\n")) {
    line = line.trim();
    if (/^[0-9]/.test(line) && hostIdPattern.test(line)) {
      const tokens = line.split(/\s+/).slice(1);
      mailerHostId = tokens[0];
      if (mailerHostId.indexOf(".") < 0) {
        for (const token of tokens) {
          if (token.indexOf(".") >= 0) {
            mailerHostId = token;
            break;
          }
        }
      }
    }
  }
  if (typeof customProps["HOSTS"] !== "undefined") {
    for (const defn of customProps["HOSTS"].split("\n")) {
      if (/^[0-9]/.test(defn) && hostIdPattern.test(defn)) {
        const tokens = defn.split(/\s+/).slice(1);
        mailerHostId = tokens[0];
        if (mailerHostId.indexOf(".") < 0) {
          for (const token of tokens) {
            if (token.indexOf(".") >= 0) {
              mailerHostId = token;
              break;
            }
          }
        }
      }
    }
  }
})
.then(() => dtc.say(`Host network ID is ${mailerHostId.toLowerCase()}`))
.then(() => dtc.say("Upload and run procedure to configure MAILER database"))
.then(() => dtc.runJob(12, 4, "opt/mailer-cfg.job", [mailerHostId]))
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
