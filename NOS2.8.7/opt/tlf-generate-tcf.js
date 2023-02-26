#!/usr/bin/env node
//
// Generate the source for the TLF configuration file.
//

const DtCyber   = require("../../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./utilities");

const dtc = new DtCyber();

const mid       = utilities.getMachineId(dtc);
const npuNode   = utilities.getNpuNode(dtc);
const topology  = utilities.getTlfTopology(dtc);
const nodeNames = Object.keys(topology).sort();

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

//
// Generate the source for the TLF configuration file.
//
let tcfSource     = [
  `TCFM${mid}.`
];

// Generate LIDS section
tcfSource = tcfSource.concat([
  "LIDS."
]);
for (const name of nodeNames) {
  let node = topology[name];
  tcfSource = tcfSource.concat([
    `${node.lid}=${node.spooler},${node.skipCount},HFRI,HFJC.`
  ]);
}

// Generate LINES section
tcfSource = tcfSource.concat([
  "LINES."
]);
for (const name of nodeNames) {
  let node = topology[name];
  tcfSource = tcfSource.concat([
    `CO${toHex(npuNode)}P${toHex(node.claPort)}=${node.spooler},${node.remoteId},${node.password},,,,${node.lid}.`
  ]);
}

tcfSource = tcfSource.concat([
  "END.",
  ""
]);

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => {
  if (nodeNames.length > 0) {
    return dtc.say("Create/update TLF configuration file ...")
    .then(() => {
      let job = [
        "$COPY,INPUT,TCFSRC.",
        "$REPLACE,TCFSRC.",
        "$PERMIT,TCFSRC,INSTALL=W."
      ];
      const options = {
        jobname:  "TCFSRC",
        username: "NETADMN",
        password: "NETADMN",
        data:     tcfSource.join("\n")
      };
      return dtc.createJobWithOutput(12, 4, job, options);
    })
  }
  else {
    return dtc.say("Purge TLF configuration file ...")
    .then(() => dtc.dis("PURGE,TCFFILE/NA.", 25));
  }
})
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
