#!/usr/bin/env node
//
// Update the MAILER network router configuration to create artifacts
// used in email routing and address translation.
//

const fs        = require("fs");
const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const hostId     = utilities.getHostId(dtc).toUpperCase();
const mailerOpts = {username:"MAILER",password:"MAILER"};
const mid        = utilities.getMachineId(dtc);
const timeZone   = utilities.getTimeZone();

let hostAliases = [`${hostId}.BITNET`];

const replaceFile = (filename, text, opts) => {
  let options = {
    jobname:  "PUTFILE",
    username: opts.username,
    password: opts.password
  };
  const body = [
    `$COPY,INPUT,${filename}.`,
    `$REPLACE,${filename}.`
  ];
  if (Array.isArray(text)) {
    options.data = dtc.asciiToCdc(text.join("\n") + "\n");
  }
  else {
    options.data = dtc.asciiToCdc(text);
  }
  return dtc.createJobWithOutput(12, 4, body, options);
}

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Discover host aliases"))
.then(() => utilities.getHostRecord(dtc))
.then(record => {
  const tokens = record.toUpperCase().split(/\s+/).slice(1);
  for (const token of tokens) {
    if (hostAliases.indexOf(token) < 0
        && token !== hostId
        && token !== "STK"
        && token.startsWith("LOCALHOST_") === false)
      hostAliases.push(token);
  }
  return Promise.resolve();
})
.then(() => dtc.say("Create routing and address translation artifacts"))
.then(() => replaceFile("ZONE",    `${timeZone}\n`,      mailerOpts))
.then(() => replaceFile("HOSTNOD", `${hostId}\n`,        mailerOpts))
.then(() => replaceFile("HOSTALI", hostAliases,          mailerOpts))
.then(() => replaceFile("HOSTSVR", `Server@${hostId}\n`, mailerOpts))
.then(() => {
  let aliasDefns = [];
  for (const alias of hostAliases) {
    aliasDefns.push(`${alias} ${hostId}`);
  }
  return replaceFile("OBXLALI", aliasDefns, mailerOpts);
})
.then(() => {
  //
  // Routes for outbound messages sent from MAILER
  //
  let routes = [
    ":nick.*DEFAULT* :route.DC=WT,UN=NETOPS,FC=OX,SCL=SY  BSMTP",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`
  ];
  for (const alias of hostAliases) {
    routes.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`);
  }
  routes.push(":nick.system :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP");
  return dtc.putFile("MAILRTE/IA", routes, mailerOpts);
})
.then(() => {
  //
  // Routes within the local domain
  //
  let routes = [
    ":nick.*DEFAULT* :njroute.BITNET BITNET",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :ccl.$BEGIN,MAILER,SYSTEM SMTP`
  ];
  for (const alias of hostAliases) {
    routes.push(`:nick.${alias} :ccl.$BEGIN,MAILER,SYSTEM SMTP`)
  }
  routes.push(":nick.system :ccl.$BEGIN,SYSTEM,SYSTEM SMTP");
  return dtc.putFile("DOMNRTE/IA", routes, mailerOpts);
})
.then(() => {
  //
  // Routes for messages received from BITNet for destinations in
  // the local domain
  //
  let routes = [
    ":nick.*DEFAULT* :njroute.BITNET BITNET",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY SMTP`
  ];
  for (const alias of hostAliases) {
    routes.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY SMTP`);
  }
  return dtc.putFile("BITNRTE/IA", routes, mailerOpts);
})
.then(() => {
  //
  // Routes for messages that have undergone inbound address translation.
  //
  let routes = [
    ":nick.*DEFAULT* :route.DC=WT,UN=NETOPS,FC=SM,SCL=SY SMTP",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`
  ];
  for (const alias of hostAliases) {
    routes.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`);
  }
  return dtc.putFile("IBXLRTE/IA", routes, mailerOpts);
})
.then(() => {
  //
  // Routes for messages that have undergone outbound address translation.
  //
  let routes = [
    ":nick.*DEFAULT* :njroute.BITNET BITNET",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`
  ];
  for (const alias of hostAliases) {
    routes.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`);
  }
  return dtc.putFile("OBXLRTE/IA", routes, mailerOpts);
})
.then(() => {
  //
  // Routes for destinations in BITNet and Internet
  //
  const topology = JSON.parse(fs.readFileSync("files/nje-topology.json"));
  const names    = Object.keys(topology).sort();
  let routes = [];
  for (const name of names) {
    let node = topology[name];
    if (typeof node.mailer !== "undefined") {
      routes.push(`:nick.${name} :mailer.${node.mailer} BSMTP`);
    }
    else if (node.software === "NJEF") {
      routes.push(`:nick.${name} :mailer.MAILER@${name} BSMTP`);
    }
    else {
      routes.push(`:nick.${name} :njroute.BITNET BITNET`);
    }
  }
  return dtc.putFile("WORLD/IA", routes, mailerOpts);
})
.then(() => dtc.runJob(12, 4, "opt/netmail-permit.job"))
.then(() => dtc.say("MAILER network routing configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
