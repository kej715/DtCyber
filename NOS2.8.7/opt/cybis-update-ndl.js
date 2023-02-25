#!/usr/bin/env node
//
// Modify the NDL file to include CYBIS/PLATO terminals.
//

const DtCyber   = require("../../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const npuNode     = utilities.getNpuNode(dtc);

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

let ndlModset = [
  "CYBIS",
  "*IDENT CYBIS",
  "*DECK NETCFG",
  "*D 85,86",
  `LI${toHex(npuNode)}P:   GROUP,    PORT=14,L9600,NI=16.`,
  `         TE${toHex(npuNode)}P:    TERMDEV,TASX364,AUTOCON,HN=${couplerNode},ABL=7.`,
  "*I 196",
  "CYBUSER: DEFINE,   MFAM=CYBER,MUSER=SYS,MAPPL=CYBIS."
];

ndlModset.push("*D 220,221");
for (let port = 0x14; port <= 0x23; port++) {
  ndlModset.push(`TE${toHex(npuNode)}P${toHex(port)}: USER,     CYBUSER.`);
}

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Generate CYBIS terminal definitions ..."))
.then(() => utilities.updateNDL(dtc, ndlModset))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
