#!/usr/bin/env node
//
// Update NDL to include NJF terminal definitions.
//

const DtCyber   = require("../../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const npuNode     = utilities.getNpuNode(dtc);
const topology    = utilities.getNjeTopology(dtc);

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

//
// Generate an NDL modset. Define a terminal for each adjacent NJE node.
//

let terminalDefns = [];
let userDefns     = [];

for (const key of Object.keys(utilities.getAdjacentNjeNodes(dtc)).sort()) {
  let node = topology[key];
  terminalDefns = terminalDefns.concat([
    `*  ${node.id}`,
    `LI${toHex(npuNode)}P${node.claPort}: LINE      PORT=${node.claPort},LTYPE=S2,TIPTYPE=TT13.`,
    `         ${node.terminalName}:  TERMDEV,STIP=USER,TC=TC29,DT=CON,ABL=7,`,
    "                           DBZ=1020,UBZ=11,DBL=7,UBL=12,XBZ=400,",
    `                           HN=${couplerNode},AUTOCON.`
  ]);
  userDefns.push(`${node.terminalName}: USER,     NJFUSER.`);
}

const ndlModset = [
  "NJF",
  "*IDENT NJF",
  "*DECK NETCFG",
  "*D 123,124"]
  .concat(terminalDefns)
  .concat([
  "*I 163",
  "NJU:     APPL,     MXCOPYS=15."
  ])
  .concat([
  "*D 229,230"
  ])
  .concat(userDefns);

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Generate terminal definitions for adjacent NJE nodes ..."))
.then(() => utilities.updateNDL(dtc, ndlModset))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
