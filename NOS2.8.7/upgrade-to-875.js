#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");
const Terminal      = require("../automation/Terminal");

const dtc  = new DtCyber();
const term = new Terminal.AnsiTerminal();

let props = {
  "cyber" : [],
  "manual": []
};
if (fs.existsSync("cyber.ovl")) {
  dtc.readPropertyFile("cyber.ovl", props);
}

let cyberText = [
  "model=CYBER875",
  "memory=20000000"
];
for (const line of props["cyber"]) {
  let ei = line.indexOf("=");
  if (ei < 0) continue;
  let key   = line.substring(0, ei).trim().toUpperCase();
  let value = line.substring(ei + 1).trim();
  if (key !== "MODEL" && key !== "MEMORY")
     cyberText.push(line);
}
props["cyber"] = cyberText;

let manualText = [
  "model=CYBER875",
  "memory=20000000"
];
for (const line of props["manual"]) {
  let ei = line.indexOf("=");
  if (ei < 0) continue;
  let key   = line.substring(0, ei).trim().toUpperCase();
  let value = line.substring(ei + 1).trim();
  if (key !== "MODEL" && key !== "MEMORY")
     manualText.push(line);
}
props["manual"] = manualText;

let lines = [];
for (const key of Object.keys(props)) {
  lines.push(`[${key}]`);
  for (const line of props[key]) {
    lines.push(`${line}`);
  }
}
lines.push("");

fs.writeFileSync("cyber.ovl", lines.join("\n"));

const ipAddress = dtc.getHostIpAddress();

dtc.start(["manual"], {
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
})
.then(() => dtc.sleep(2000))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.console("idle off"))
.then(() => dtc.say("DtCyber started using manual profile"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Begin initial deadstart ..."))
.then(() => dtc.dsd([
  "O!",
  "#1000#P!",
  "#1000#D=YES",
  "#1000#",
  "#5000#NEXT.",
  "#1000#]!",
  "#1000#EQ005=DE,ET=EM,SZ=10000.",
  "#1000#INITIALIZE,AL,5,6.",
  "#1000#GO.",
  "#5000#%year%%mon%%day%",
  "#3000#%hour%%min%%sec%"
]))
.then(() => dtc.expect([ {re:/QUEUE FILE UTILITY COMPLETE/} ], "printer"))
.then(() => dtc.say("Initial deadstart of Cyber 875 complete"))
.then(() => dtc.say("Wait for NAM and IAF to start ..."))
.then(() => dtc.sleep(10000))
.then(() => term.connect(`${ipAddress}:23`))
.then(() => term.say("Login ..."))
.then(() => term.loginNOS2("", "INSTALL", "INSTALL"))
.then(() => term.say("Logged in"))
.then(() => term.send("COMMON,SYSTEM\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.send("GTR,SYSTEM,CMRD01.CMRD01\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.say("Extracted CMRD01 from SYSTEM"))
.then(() => term.send("XEDIT,CMRD01\r"))
.then(() => term.expect([{ re: /\?\? / }]))
.then(() => term.send("CS/M01 - CYBER 865/M01 - CYBER 875/\r"))
.then(() => term.expect([{ re: /\?\? / }]))
.then(() => term.send("Q\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.say("Updated CMRD01"))
.then(() => term.send("GTR,SYSTEM,EQPD01.EQPD01\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.say("Extracted EQPD01 from SYSTEM"))
.then(() => term.send("XEDIT,EQPD01\r"))
.then(() => term.expect([{ re: /\?\? / }]))
.then(() => term.send("CS/ET=EM,SZ=2400/ET=EM,SZ=10000/\r"))
.then(() => term.expect([{ re: /\?\? / }]))
.then(() => term.send("Q\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.say("Updated EQPD01"))
.then(() => term.send("COPY,CMRD01,LGO\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.send("COPY,EQPD01,LGO\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.say("Update PRODUCT with updated CMRD01 and EQPD01 ..."))
.then(() => term.send("ATTACH,PRODUCT/M=W,WB\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.send("LIBEDIT,P=PRODUCT,B=LGO,I=0,C\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.say("Done"))
.then(() => term.send("LOGOUT\r"))
.then(() => term.expect([{ re: /CHARACTERS=/ }]))
.then(() => term.disconnect())
.then(() => term.say("Logged out"))
.then(() => dtc.disconnect())
.then(() => dtc.say("Make a new deadstart tape ..."))
.then(() => dtc.exec("node", ["make-ds-tape"]))
.then(() => dtc.say("Shutdown system to deadstart using new tape ..."))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.shutdown(false))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Save previous deadstart tape and rename new one ..."))
.then(() => {
  if (fs.existsSync("tapes/ods.tap")) {
    fs.unlinkSync("tapes/ods.tap");
  }
  fs.renameSync("tapes/ds.tap", "tapes/ods.tap");
  fs.renameSync("tapes/newds.tap", "tapes/ds.tap");
  return Promise.resolve();
})
.then(() => dtc.say("Deadstart system using new tape"))
.then(() => dtc.start({
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
}))
.then(() => dtc.sleep(5000))
.then(() => dtc.connect())
.then(() => dtc.expect([{ re: /Operator> $/ }]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.say(""))
.then(() => dtc.say("Upgrade to Cyber 875 complete"))
.then(() => dtc.say(""))
.then(() => dtc.say("Enter 'exit' command to exit and shutdown gracefully"))
.then(() => dtc.engageOperator(cmdExtensions))
.then(() => dtc.shutdown())
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
