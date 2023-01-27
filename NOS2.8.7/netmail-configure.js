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
  return dtc.say(`  ${filename}`)
  .then(() => dtc.createJobWithOutput(12, 4, body, options));
}

let hostAliases = [`${hostId}.BITNET`];

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
  return dtc.say("  MAILRTE")
  .then(() => dtc.putFile("MAILRTE/IA", routes, mailerOpts));
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
  return dtc.say("  DOMNRTE")
  .then(() => dtc.putFile("DOMNRTE/IA", routes, mailerOpts));
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
  return dtc.say("  BITNRTE")
  .then (() => dtc.putFile("BITNRTE/IA", routes, mailerOpts));
})
.then(() => {
  //
  // Routes for messages that have undergone inbound address translation.
  //
  let routes = [
    ":nick.*DEFAULT* :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`
  ];
  for (const alias of hostAliases) {
    routes.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=ID,SCL=SY SMTP`);
  }
  return dtc.say("  IBXLRTE")
  .then (() => dtc.putFile("IBXLRTE/IA", routes, mailerOpts));
})
.then(() => {
  //
  // Routes for messages that have undergone outbound address translation.
  //
  let routes = [
    ":nick.*DEFAULT* :njroute.BITNET BITNET",
    ":nick.*<>*      :route.DC=WT,UN=MAILER,FC=UK,UJN=UNKNOWN  RFC822",
    `:nick.${hostId} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY SMTP`
  ];
  for (const alias of hostAliases) {
    routes.push(`:nick.${alias} :route.DC=WT,UN=NETOPS,FC=IX,SCL=SY SMTP`);
  }
  return dtc.say("  OBXLRTE")
  .then (() => dtc.putFile("OBXLRTE/IA", routes, mailerOpts));
})
.then(() => {
  //
  // Routes for destinations in BITNet and Internet
  //
  let routes        = [];
  const topology    = JSON.parse(fs.readFileSync("files/nje-topology.json"));
  const customProps = {};
  dtc.readPropertyFile(customProps);
  //
  // 1. Add/replace NJE node definitions from custom properties
  //
  if (typeof customProps["NETWORK"] !== "undefined") {
    for (let line of customProps["NETWORK"]) {
      line = line.toUpperCase();
      let ei = line.indexOf("=");
      if (ei < 0) continue;
      let key   = line.substring(0, ei).trim();
      let value = line.substring(ei + 1).trim();
      if (key === "NJENODE") {
        //
        //  njeNode=<nodename>,<software>,<lid>,<public-addr>,<link>
        //     [,<local-address>][,B<block-size>][,P<ping-interval>][,<mailer-address>]
        let items = value.split(",");
        if (items.length >= 5) {
          let nodeName = items.shift();
          let node = {
            software: items.shift(),
            lid: items.shift(),
            publicAddress: items.shift(),
            link: items.shift()
          };
          if (items.length > 0) {
            node.localAddress = items.shift();
          }
          while (items.length > 0) {
            let item = items.shift();
            if (item.startsWith("B")) {
              node.blockSize = parseInt(item.substring(1));
            }
            else if (item.startsWith("P")) {
              node.pingInterval = parseInt(item.substring(1));
            }
            else if (item.indexOf("@") > 0) {
              node.mailer = item;
            }
          }
          topology[nodeName] = node;
        }
      }
    }
  }
  //
  // 2. Create routes for SMTP destinations defined in HOSTS property
  //
  if (typeof customProps["HOSTS"] !== "undefined") {
    for (const defn of customProps["HOSTS"]) {
      if (/^[0-9]/.test(defn)) {
        const names = defn.split(/\s+/).slice(1);
        for (name of names) {
          let ucname = name.toUpperCase();
          if (ucname !== hostId
              && hostAliases.indexOf(ucname) < 0
              && typeof topology[ucname] === "undefined"
              && ucname !== "STK"
              && ucname.startsWith("LOCALHOST_") === false) {
            routes.push(`:nick.${name} :route.DC=WT,UN=NETOPS,FC=SM,SCL=SY SMTP`);
          }
        }
      }
    }
  }
  //
  // 3. Create routes for NJE destinations defined in topology
  //
  const names = Object.keys(topology).sort();
  for (const name of names) {
    if (name === hostId) continue;
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
  return dtc.say("  WORLD")
  .then(() => dtc.putFile("WORLD/IA", routes, mailerOpts));
})
.then(() => dtc.say("Permit artifacts to SYSTEMX ..."))
.then(() => dtc.runJob(12, 4, "opt/netmail-permit.job"))
.then(() => dtc.say("MAILER network routing configuration completed successfully"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
