#!/usr/bin/env node
//
// Update the MAILER network router configuration to create artifacts
// used in email routing and address translation.
//

const DtCyber     = require("../automation/DtCyber");
const fs          = require("fs");
const utilities   = require("./opt/utilities");

const dtc = new DtCyber();

const customProps = utilities.getCustomProperties(dtc);
const hostId      = utilities.getHostId(dtc);
const mid         = utilities.getMachineId(dtc);
const timeZone    = utilities.getTimeZone();
const mailerOpts  = {
  username:"MAILER",
  password:utilities.getPropertyValue(customProps, "PASSWORDS", "MAILER", "MAILER")
};
let hostAliases   = [`${hostId}.BITNET`];

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Discover host aliases ..."))
.then(() => utilities.getHostRecord(dtc))
.then(record => {
  const names = record.toUpperCase().split(/\s+/).slice(1);
  for (const name of names) {
    if (hostAliases.indexOf(name) < 0
        && name !== hostId
        && name !== "STK"
        && name.startsWith("LOCALHOST_") === false)
      hostAliases.push(name);
  }
  return Promise.resolve();
})
.then(() => dtc.say("Create routing and address translation artifacts ..."))
.then(() => utilities.getFile(dtc, "TCPHOST/UN=NETADMN"))
.then(hostsText => {
  hostsText = dtc.cdcToAscii(hostsText);
  let proc = [
    ".PROC,TABLES.",
    "$REPLACE,HOSTNOD,HOSTALI,HOSTSVR,OBXLALI,OBXLRTE.",
    "$REPLACE,MAILRTE,DOMNRTE,BITNRTE,IBXLRTE.",
    "$REPLACE,NJERTE,RHPRTE,SMTPRTE,ZONE.",
    "$CHANGE,HOSTNOD,HOSTALI,HOSTSVR,OBXLALI,OBXLRTE/CT=PU,M=R.",
    "$CHANGE,MAILRTE,DOMNRTE,BITNRTE,IBXLRTE/CT=PU,M=R.",
    "$CHANGE,NJERTE,RHPRTE,SMTPRTE,ZONE/CT=PU,M=R.",
    ".DATA,ZONE.",
    `${timeZone}`,
    ".DATA,HOSTNOD.",
    `${hostId}`,
    ".DATA,HOSTSVR.",
    `Server@${hostId}`
  ];

  proc.push(".DATA,HOSTALI.");
  proc = proc.concat(hostAliases);

  proc.push(".DATA,OBXLALI.");
  for (const alias of hostAliases) {
    proc.push(`${alias} ${hostId}`);
  }

  proc.push(".DATA,MAILRTE.");
  proc = proc.concat([
    "*",
    "* Routes for outbound messages sent from MAILER",
    "*",
    ":nick.*DEFAULT* :route.DC=WT,UN=NETOPS,FC=OX,SCL=SY  BSMTP",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`
  ]);
  for (const alias of hostAliases) {
    proc.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`);
  }
  proc.push(":nick.system :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP");

  proc.push(".DATA,DOMNRTE.");
  proc = proc.concat([
    "*",
    "* Routes within the local domain",
    "*",
    ":nick.*DEFAULT* :njroute.BITNET BITNET",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :ccl.$BEGIN,MAILER,SYSTEM SMTP`
  ]);
  for (const alias of hostAliases) {
    proc.push(`:nick.${alias} :ccl.$BEGIN,MAILER,SYSTEM SMTP`)
  }
  proc.push(":nick.system :ccl.$BEGIN,SYSTEM,SYSTEM SMTP");

  proc.push(".DATA,BITNRTE.");
  proc = proc.concat([
    "*",
    "* Routes for messages received from NJE and SMTP for destinations in",
    "* the local domain",
    "*",
    ":nick.*DEFAULT* :njroute.BITNET BITNET",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY SMTP`
  ]);
  for (const alias of hostAliases) {
    proc.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY SMTP`);
  }

  proc.push(".DATA,IBXLRTE.");
  proc = proc.concat([
    "*",
    "* Routes for messages that have undergone inbound address translation.",
    "*",
    ":nick.*DEFAULT* :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`
  ]);
  for (const alias of hostAliases) {
    proc.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`);
  }

  proc.push(".DATA,OBXLRTE.");
  proc = proc.concat([
    "*",
    "* Routes for messages that have undergone outbound address translation.",
    "*",
    ":nick.*DEFAULT* :njroute.BITNET BITNET",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY SMTP`
  ]);
  for (const alias of hostAliases) {
    proc.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY SMTP`);
  }

  proc.push(".DATA,NJERTE.");
  proc = proc.concat([
    "*",
    "* Routes for destinations reached by NJE",
    "*",
    ":nick..BITNET :njroute.BITNET BITNET"
  ]);
  const njeTopology = utilities.getNjeTopology(dtc);
  let names = Object.keys(njeTopology).sort();
  for (const name of names) {
    if (name === hostId) continue;
    let node = njeTopology[name];
    if (typeof node.mailer !== "undefined") {
      proc.push(`:nick.${name} :mailer.${node.mailer} BSMTP`);
    }
    else if (node.software === "NJEF") {
      proc.push(`:nick.${name} :mailer.MAILER@${name} BSMTP`);
    }
    else {
      proc.push(`:nick.${name} :njroute.BITNET BITNET`);
    }
  }

  proc.push(".DATA,RHPRTE.");
  proc = proc.concat([
    "*",
    "* Routes for destinations reached by RHP",
    "*"
  ]);
  const rhpTopology = utilities.getRhpTopology(dtc);
  names = Object.keys(rhpTopology).sort();
  for (const name of names) {
    if (name === hostId) continue;
    let node = rhpTopology[name];
    proc.push(`:nick.${name} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY,ST=${node.lid} SMTP`);
    let hostRecord = null;
    for (let line of hostsText.split("\n")) {
      line = line.trim();
      if (/^[0-9]/.test(line)) {
        const tokens = line.toUpperCase().split(/\s+/);
        if (tokens.indexOf(name) !== -1) {
          hostRecord = line;
        }
      }
    }
    const customProps = utilities.getCustomProperties(dtc);
    if (typeof customProps["HOSTS"] !== "undefined") {
      for (let defn of customProps["HOSTS"]) {
        if (/^[0-9]/.test(defn)) {
          const tokens = defn.toUpperCase().split(/\s+/);
          if (tokens.indexOf(name) !== -1) {
            hostRecord = defn;
          }
        }
      }
    }
    if (hostRecord !== null) {
      let aliases = [];
      for (const token of hostRecord.split(/\s+/).slice(1)) {
        let ucname = token.toUpperCase();
        if (   ucname !== name
            && ucname !== "STK"
            && ucname !== "MAIL-RELAY"
            && ucname.startsWith("LOCALHOST_") === false) {
          proc.push(`:nick.${token} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY,ST=${node.lid} SMTP`);
        }
      }
    }
  }

  proc.push(".DATA,SMTPRTE.");
  proc = proc.concat([
    "*",
    "* Routes for destinations reached by SMTP",
    "*"
  ]);
  
  let domains = ["at" ,"au" ,"ca" ,"ch" ,"com","de" ,"edu","es" ,"fr" ,"gb" ,
                 "ie" ,"il" ,"it" ,"jp" ,"mil","net","org","oz" ,"uk" ,"us"];
  let smtpRoutes = {};
  for (const dom of domains) {
    smtpRoutes[`.${dom}`] = `:nick..${dom} :route.DC=WT,UN=NETOPS,FC=SM,SCL=SY SMTP`;
  }
  if (typeof customProps["HOSTS"] !== "undefined") {
    for (const defn of customProps["HOSTS"]) {
      if (/^[0-9]/.test(defn)) {
        const names = defn.split(/\s+/).slice(1);
        for (name of names) {
          let ucname = name.toUpperCase();
          if (ucname !== hostId
              && hostAliases.indexOf(ucname) < 0
              && typeof njeTopology[ucname] === "undefined"
              && typeof rhpTopology[ucname] === "undefined"
              && ucname !== "STK"
              && ucname !== "MAIL-RELAY"
              && ucname.startsWith("LOCALHOST_") === false) {
            smtpRoutes[name] = `:nick.${name} :route.DC=WT,UN=NETOPS,FC=SM,SCL=SY SMTP`;
          }
        }
      }
    }
  }
  if (typeof customProps["NETWORK"] !== "undefined") {
    for (let line of customProps["NETWORK"]) {
      let ei = line.indexOf("=");
      if (ei < 0) continue;
      let key   = line.substring(0, ei).trim();
      let value = line.substring(ei + 1).trim();
      if (key.toUpperCase() === "SMTPDOMAIN") {
        //
        //  smtpDomain=<domainname>
        //
        smtpRoutes[value] = `:nick.${value} :route.DC=WT,UN=NETOPS,FC=SM,SCL=SY SMTP`;
      }
    }
  }
  for (const dom of Object.keys(smtpRoutes).sort()) {
    proc.push(smtpRoutes[dom]);
  }

  let options = {
    jobname:  "TABLES",
    username: "MAILER",
    password: utilities.getPropertyValue(customProps, "PASSWORDS", "MAILER", "MAILER")
  };
  const job = [
    "$GET,TBLPROC.",
    "$PURGE,TBLPROC.",
    "TBLPROC."
  ];
  
  return dtc.say("Upload MAILER routing tables ...")
  .then(() => dtc.putFile("TBLPROC/IA", proc, mailerOpts))
  .then(() => dtc.createJobWithOutput(12, 4, job, options));
})
.then(() => dtc.say("MAILER network routing configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
