#!/usr/bin/env node
//
// Update the NDL source to include definitions for the TLF configuration.
//

const DtCyber   = require("../../automation/DtCyber");
const utilities = require("./utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const npuNode     = utilities.getNpuNode(dtc);
const topology    = utilities.getTlfTopology(dtc);

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

//
// Update network parameters to reflect customizations, if any.
//
let terminalDefns = [];
let userDefns     = [];

const nodeNames = Object.keys(topology).sort();
for (const name of nodeNames) {
  let node      = topology[name];
  let console   = `CO${toHex(npuNode)}P${toHex(node.claPort)}`;
  terminalDefns = terminalDefns.concat([
    `*  ${name}`,
    `LI${toHex(npuNode)}P${toHex(node.claPort)}: LINE      PORT=${toHex(node.claPort)},LTYPE=S2,TIPTYPE=TT12.`,
    "         TERMINAL  TC=TC28,STIP=USER.",
    `         ${console}:  DEVICE,REVHASP,HN=${couplerNode},DT=CON,ABL=2,DBZ=120,PW=120.`,
    `         CR${toHex(npuNode)}P${toHex(node.claPort)}:  DEVICE,REVHASP,HN=${couplerNode},DT=CR,DO=1,TA=1,ABL=3,DBZ=810,`,
    "                 PW=137.",
    `         LP${toHex(npuNode)}P${toHex(node.claPort)}:  DEVICE,REVHASP,HN=${couplerNode},DT=LP,DO=1,TA=1,ABL=0,DBZ=0.`,
    `         PL${toHex(npuNode)}P${toHex(node.claPort)}:  DEVICE,REVHASP,HN=${couplerNode},DT=PL,DO=1,TA=1,ABL=0,DBZ=0.`
  ]);
  userDefns.push(`${console}: USER,     TLFUSER.`);
}

const ndlModset = [
  "TLF",
  "*IDENT TLF",
  "*DECK NETCFG",
  "*I 42",
  " ",
  "REVHASP: DEFINE,   AUTOCON, DBL=7, UBL=7, UBZ=100, XBZ=400.",
  "*D 114,115"]
  .concat(terminalDefns)
  .concat([
  "*D 226,227"
  ])
  .concat(userDefns)
  .concat("");

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Generate NDL for TLF terminal definitions ..."))
.then(() => utilities.updateNDL(dtc, ndlModset))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
