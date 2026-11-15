#!/usr/bin/env node

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./opt/utilities");

const dtc  = new DtCyber();

const config = utilities.getNhpConfiguration(dtc);
let configKeys = [];
for (const key of Object.keys(config)) {
  if (key.startsWith("MDI") || key.startsWith("NPU")) configKeys.push(key);
}
configKeys = configKeys.sort((k1, k2) => {
  return config[k1].couplerNode - config[k2].couplerNode;
});

let productRecords = [];

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Edit EQPD01 ..."))
.then(() => utilities.getSystemRecord(dtc, "EQPD01"))
.then(eqpd01 => {
  //
  //  Collect all equipment with ordinals between 040 and 047 that are not
  //  MDI's or NPU's.
  //
  let outliers = [];
  for (const line of eqpd01.split("\n")) {
    if (line.startsWith("EQ")) {
      let ei  = line.indexOf("=");
      let eqn = parseInt(line.substring(2, ei), 8);
      if (eqn >= 0o40 && eqn <= 0o47) {
        if (!/^EQ[0-7]+=ND/.test(line) && !/^EQ[0-7]+=NP/.test(line)) {
          outliers.push(line.substring(ei + 1));
        }
      }
    }
  }
  let eqDefns = [];
  let eqNo    = 0o40;
  for (const key of configKeys) {
    let device = config[key];
    let defn = `EQ0${eqNo.toString(8)}=`;
    if (key.startsWith("MDI")) {
      defn += `ND,CH=${device.channelNo.toString(8)}`
           +    `,EQ=${device.eqNo.toString(8)}`
           +    `,ND=${device.couplerNode.toString(8)}`
           +    `,PI=${device.pipNo.toString(8)}`;
      if (key === "MDI0") {
        defn += `,NT=${device.cdcnetNode.toString(8)}`
      }
    }
    else {
      defn += `NP,CH=${device.channelNo.toString(8)}`
           +    `,EQ=${device.eqNo.toString(8)}`
           +    `,ND=${device.couplerNode.toString(8)}`
           +    `,PI=${device.pipNo.toString(8)}`
           +    `,SA=ON`
    }
    eqDefns.push(`${defn}.`);
    eqNo += 1;
  }
  for (const outlier of outliers) {
    eqDefns.push(`EQ0${eqNo.toString(8)}=${outlier}`);
    eqNo += 1;
  }
  eqpd01 = utilities.editEqpdProps(eqpd01, eqDefns);;
  productRecords.push(eqpd01);
  console.log(eqpd01);
  return Promise.resolve();
})
.then(() => utilities.updateProductRecords(dtc, productRecords))
.then(() => dtc.say("EQPD01 updated and replaced in PRODUCTS file"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  process.stderr.write(`${err}\n`);
  process.exit(1);
});
