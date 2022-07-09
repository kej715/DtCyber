#!/usr/bin/env node

const DtCyber = require("../automation/DtCyber");

const dtc = new DtCyber();

dtc.start("./dtcyber", ["manual"])
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("DtCyber started"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Begin initial deadstart ..."))
.then(() => dtc.dsd([
  "O!",
  "#500#P!",
  "#500#D=YES",
  "#500#",
  "#1000#NEXT.",
  "#1000#]!",
  "#1000#INITIALIZE,AL,5,6,10,11,12,13.",
  "#500#GO.",
  "#1500#%year%%mon%%day%",
  "#1500#%hour%%min%%sec%"
]))
.then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
.then(() => dtc.say("Initial deadstart complete"))
.then(() => dtc.say("Launching SYSGEN(FULL) ..."))
.then(() => dtc.mount(13, 0, 1, "tapes/nos287-1.tap"))
.then(() => dtc.mount(13, 0, 2, "tapes/nos287-2.tap"))
.then(() => dtc.mount(13, 0, 3, "tapes/nos287-3.tap"))
.then(() => dtc.dsd("X.SYSGEN(FULL)"))
.then(() => dtc.expect([ {re:/E N D   F U L L/} ], "printer"))
.then(() => dtc.say("SYSGEN complete"))
.then(() => dtc.say("Create and compile the NDL file ..."))
.then(() => dtc.runJob(12, 4, "decks/create-ndlopl.job"))
.then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job"))
.then(() => dtc.say("Update the NAMSTRT file ..."))
.then(() => dtc.dis("PERMIT,NAMSTRT,INSTALL=W.", 377772))
.then(() => dtc.runJob(12, 4, "decks/update-namstrt.job"))
.then(() => dtc.say("Create the TCPHOST file ..."))
.then(() => dtc.runJob(12, 4, "decks/create-tcphost.job"))
.then(() => dtc.say("Add terminal definitions to TERMLIB ..."))
.then(() => dtc.dis("PERMIT,TERMLIB,INSTALL=W.", 377776))
.then(() => dtc.runJob(12, 4, "decks/update-termlib.job"))
.then(() => dtc.say("Add a non-privileged GUEST user ..."))
.then(() => dtc.dsd("X.MS(VALUSER,LIMITED,GUEST,GUEST)"))
.then(() => dtc.say("\n\n===== First installation completed successfully =====\n"))
.then(() => dtc.shutdown())
.catch(err => {
  console.log(`Error caught: ${err}`);
  process.exit(1);
});
