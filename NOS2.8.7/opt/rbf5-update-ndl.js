#!/usr/bin/env node
//
// Update the NDL source to include definitions for RBF terminals
//

const DtCyber   = require("../../automation/DtCyber");
const utilities = require("./utilities");

const dtc = new DtCyber();

const couplerNode = utilities.getCouplerNode(dtc);
const npuNode     = utilities.getNpuNode(dtc);
const terminals   = utilities.getHaspTerminals(dtc);

const toHex = value => {
  return value < 16 ? `0${value.toString(16)}` : value.toString(16);
};

//
// Update network parameters to reflect customizations, if any.
//
let terminalDefns = [];
let userDefns     = [];

if (terminals.length < 1) process.exit(0);
for (const terminal of terminals) {
  let console   = `CO${toHex(npuNode)}P${toHex(terminal.claPort)}`;
  terminalDefns = terminalDefns.concat([
    `*  ${terminal.id}`,
    `LI${toHex(npuNode)}P${toHex(terminal.claPort)}: LINE      PORT=${toHex(terminal.claPort)},LTYPE=S2,TIPTYPE=HASP.`,
    `         TERMINAL  TC=${terminal.termClass},RIC=YES.`,
    `         ${console}:  DEVICE,DT=CON,HN=${couplerNode},PW=0,AUTOCON.`,
    `         CR${toHex(npuNode)}P${toHex(terminal.claPort)}:  DEVICE,DT=CR,STREAM=1.`,
    `         LP${toHex(npuNode)}P${toHex(terminal.claPort)}:  DEVICE,DT=LP,STREAM=1,SDT=A9,PW=132.`,
    `         PU${toHex(npuNode)}P${toHex(terminal.claPort)}:  DEVICE,DT=CP,STREAM=1.`,
    ""
  ]);
  userDefns.push(`${console}: USER,     RBFUSER,MUSER=${terminal.id}.`);
}

const ndlModset = [
  "HASP",
  "*IDENT HASP",
  "*DECK NETCFG",
  "*I 107"]
  .concat(terminalDefns)
  .concat([
  "*D 225"
  ])
  .concat(userDefns)
  .concat("");

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Generate NDL for RBF terminal definitions ..."))
.then(() => utilities.updateNDL(dtc, ndlModset))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
