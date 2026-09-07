#!/usr/bin/env node

const cmdExtensions = require("./cmd-extensions");
const DtCyber       = require("../automation/DtCyber");
const fs            = require("fs");
const utilities     = require("./opt/utilities");

const usage = () => {
  process.stderr.write("Usage: node upgrade-to-870 [-nve] [-auto]\n");
  process.stderr.write("  -nve   do NOT install NOS/VE\n");
  process.stderr.write("  -auto  do NOT update IPR deck to start NOS/VE automatically\n");
  process.exit(1);
};

let installNVE = true;
let autoNVE    = true;

for (let i = 2; i < process.argv.length; i++) {
  if (process.argv[i] === "-nve") {
    installNVE = false;
  }
  else if (process.argv[i] === "-auto") {
    autoNVE = false;
  }
  else if (process.argv[i] === "-h" || process.argv[i] === "-help" || process.argv[i] === "--help") {
    usage();
    process.exit(0);
  }
  else {
    usage();
    process.exit(1);
  }
}

const dtc  = new DtCyber();

const media = [
  {name: "CIP870L847.tap",        dir: "tapes",     url: "https://www.dropbox.com/scl/fi/5gdf3nqu0knwe89ieh78h/CIP870L847.tap?rlkey=bonu7ta49l5u39aigjqfotpom&dl=1"},
  {name: "nve857ds.tap",          dir: "tapes",     url: "https://www.dropbox.com/scl/fi/4zpm2v5gg64381awjb31n/nve857ds.tap?rlkey=s215s8hks3z5e1zbo7n7tpquz&dl=1"},
  {name: "dual-state.tap",        dir: "opt/tapes", url: "https://www.dropbox.com/scl/fi/646fpu437vhoonv9mc7uk/dual-state.tap?rlkey=id2gnoddnr5866ywoyrc584hf&dl=1"}
];

const cmrdProps = [
  "VE=67720."  // ~111 Mbytes
];

const eqpdProps = [
  "EQ005=DE,ET=EM,SZ=4000.",  // 2048K words UEM
  /*
  "EQ100=DQ,UN=0,CH=20,ST=DOWN.",
  "EQ101=DQ,UN=1,CH=22,ST=DOWN.",
  "EQ102=DQ,UN=2,CH=20,ST=DOWN.",
  "EQ103=DQ,UN=3,CH=22,ST=DOWN.",
  "EQ104=NT-2,UN=0,CH=21,TF=ATS,ST=DOWN.",
  "DOWN,CH=20,21,22.",
  */
  "UEMIN."
];

const updateIniFile = (nosDsTape) => {
  let iniProps = dtc.getIniProperties();
  let tapeProps = [];
  for (const prop of iniProps["cyber"]) {
    tapeProps.push(prop);
  }

  let props = {
    "cyber" : [],
    "manual": []
  };
  props["tape"] = tapeProps;

  if (fs.existsSync("cyber.ovl")) {
    dtc.readPropertyFile("cyber.ovl", props);
  }

  for (const sectionKey of ["cyber", "manual", "tape"]) {
    utilities.putPropertyValue(props, sectionKey, "model",    "CYBER870");
    utilities.putPropertyValue(props, sectionKey, "cpus",     "2");
    utilities.putPropertyValue(props, sectionKey, "pps",      "24");
    utilities.putPropertyValue(props, sectionKey, "memory",   "128M");
    utilities.putPropertyValue(props, sectionKey, "esmbanks", "0");
    utilities.putPropertyValue(props, sectionKey, "osType",   "NOS");
    utilities.putPropertyValue(props, sectionKey, "clock",    "0");
    utilities.putPropertyValue(props, sectionKey, "idle",     "off");
    utilities.putPropertyValue(props, sectionKey, "helpers",  "helpers.dual-state");
  }
  utilities.putPropertyValue(props, "cyber",  "deadstart", "deadstart.dual-state.disk");
  utilities.putPropertyValue(props, "cyber",  "operator",  "operator.dual-state.auto");
  utilities.putPropertyValue(props, "manual", "deadstart", "deadstart.dual-state.disk");
  utilities.putPropertyValue(props, "manual", "operator",  "operator.dual-state.manual");
  utilities.putPropertyValue(props, "tape",   "deadstart", "deadstart.dual-state.tape");
  utilities.putPropertyValue(props, "tape",   "operator",  "operator.dual-state.manual");
  if (typeof props["equipment.nos287"] === "undefined") {
    props["equipment.nos287"] = [];
  }
  let eqDefns = [];
  eqDefns.push(`MT679,0,0,13,${nosDsTape}`);
  for (const defn of props["equipment.nos287"]) {
    if (defn.startsWith("DD885-42")) {
      eqDefns.push(`DD885${defn.substring(8)}`);
    }
    else if (!defn.startsWith("DD885-LS")
        && !defn.startsWith("TPM")
        && !defn.startsWith("MT679,0,0,13")
        && !defn.startsWith("MT679,0,0,21")
        && !defn.startsWith("MT679,0,1,21")) {
      eqDefns.push(defn);
    }
  }
  eqDefns.push("TPM,0,0,15,6602,6603");
  eqDefns.push("DD885-LS,0,0,20,disks/NVE01");
  eqDefns.push("DD885-LS,0,1,22,disks/NVE02");
  eqDefns.push("DD885-LS,0,2,20,disks/NVE03");
  eqDefns.push("DD885-LS,0,3,22,disks/NVE04");
  eqDefns.push("MT679,0,0,21,tapes/nve857ds.tap");
  eqDefns.push("MT679,0,1,21");
  props["equipment.nos287"] = eqDefns;

  props["helpers.dual-state"] = [
    "./console-server",
    "./rjews",
    "./stk",
    "./webterm-server",
    "./nve-console"
  ];

  props["operator.dual-state.auto"] = [
    "set_operator_port 6662",
    "enter_keys #2000#"
  ];

  props["operator.dual-state.manual"] = [
    "set_operator_port 6662",
  ];

  props["deadstart.dual-state.disk"] = [
    "1402 LDN 02",
    "7301 OAM 01,",
    "0017        0017",
    "7541 DCN 01",
    "7701 FNC 01,",
    "0300        0300",
    "7401 ACN 01",
    "7101 IAM 01,",
    "7301        7301",
    "0004 Cyber 840 - 995",
    "0001 wxyy w=level, x=display, yy=cmrdeck",
    "0000",
    "0000",
    "0000",
    "0000",
    "7112"
  ];

  props["deadstart.dual-state.tape"] = [
    "0000",
    "0000",
    "0000",
    "7553 DCN 13",
    "7713 FNC 13,",
    "0120        0120",
    "7413 ACN 13",
    "7113 IAM 13,",
    "7301        7301",
    "0000",
    "0001 wxyy w=level, x=display, yy=cmrdeck",
    "0000"
  ];

  utilities.writePropertyFile("cyber.ovl", props);
};

//
//  Download dual-state media
//
let progressMaxLen = 0;
let promise = dtc.say("Download dual-state media ...");
for (const m of media) {
  promise = promise
  .then(() => dtc.say(`  ${m.name} ...`))
  .then(() => dtc.wget(m.url, m.dir, m.name, (byteCount, contentLength) => {
    let progress = `\r${new Date().toLocaleTimeString()}   Received ${byteCount}`;
    if (contentLength === -1) {
      progress += " bytes";
    }
    else {
      progress += ` of ${contentLength} bytes (${Math.round((byteCount / contentLength) * 100)}%)`;
    }
    if (progress.length > progressMaxLen) progressMaxLen = progress.length;
    process.stdout.write(progress)
  }))
  .then(() => new Promise((resolve, reject) => {
    let progress = `\r`;
    while (progress.length++ < progressMaxLen) progress += " ";
    process.stdout.write(`${progress}\r`);
    resolve();
  }));
}

let productRecords = [];
let dumpTape = `tapes/pfdump-${new Date().toISOString().replaceAll(':', '')}.tap`;

promise = promise
.then(() => dtc.say("All media downloaded"))
.then(() => dtc.start({
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
}))
//
//  Deadstart NOS and edit the CMR, EQP, and IPR decks
//
.then(() => dtc.sleep(5000))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.console("idle off"))
.then(() => dtc.say("DtCyber started - deadstarting NOS 2.8.7"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.say("Deadstart complete"))
.then(() => dtc.say("Edit CMRD01 ..."))
.then(() => utilities.getSystemRecord(dtc, "CMRD01"))
.then(cmrd01 => {
  cmrdProps.push(`NAME=M${utilities.getMachineId(dtc)} - DUAL-STATE CYBER 870.`);
  cmrd01 = utilities.editCmrdProps(cmrd01, cmrdProps);
  productRecords.push(cmrd01);
  console.log(cmrd01);
})
.then(() => dtc.say("Edit EQPD01 ..."))
.then(() => utilities.getSystemRecord(dtc, "EQPD01"))
.then(eqpd01 => {
  eqpdProps.push(`XM=${utilities.getMachineId(dtc)},0,1000.`); // XM=<mid>,0,1000.  512K words of user EM
  eqpd01 = utilities.editEqpdProps(eqpd01, eqpdProps);
  //
  //  Remove equipment definitions related to ECS/ESM and two-port mux, and change
  //  any definitions for disk equipment type DB to DQ.
  //
  let dpEqn = -1;
  let lines = [];
  for (const line of eqpd01.split("\n")) {
    if (/^EQ[0-7]+=RM/.test(line)) continue;
    if (/^EQ[0-7]+=DP/.test(line)) {
      let ei = line.indexOf("=");
      dpEqn = parseInt(line.substring(2, ei), 8);
      continue;
    }
    if (/^EQ[0-7]+=DB/.test(line)) {
      let ei = line.indexOf("=");
      let eqn = parseInt(line.substring(2, ei), 8);
      if (eqn >= 0o010 && eqn <= 0o013) {
        lines.push(`${line.substring(0, ei + 1)}DQ${line.substring(ei + 3)}`);
        continue;
      }
    }
    if (line.startsWith("ASR") || line.startsWith("MSAL,S=")) {
      let ei = line.indexOf("=");
      let pi = line.indexOf(".");
      if (dpEqn === parseInt(line.substring(ei + 1, pi), 8)) continue;
    }
    else if (line.startsWith("PF")) {
      let ei = line.indexOf("=");
      let ci = line.indexOf(",");
      if (dpEqn === parseInt(line.substring(ei + 1, ci), 8)) continue;
    }
    lines.push(line);
  }
  eqpd01 = lines.join("\n");
  productRecords.push(eqpd01);
  console.log(eqpd01);
})
.then(() => dtc.say("Edit IPRD01 ..."))
.then(() => utilities.getSystemRecord(dtc, "IPRD01"))
.then(iprd01 => {
  //
  //  Set NVE subsystem to start at control point 3, and move CYBIS
  //  to control point 4 if it's defined in the deck.
  //
  let lines = [];
  let isNveDefined = false;
  for (const line of iprd01.split("\n")) {
    if (line.startsWith("ENABLE,CYB,3.")) {
      lines.push("ENABLE,CYB,4.");
    }
    else if (line.startsWith("DISABLE,CYB,3.")) {
      lines.push("DISABLE,CYB,4.");
    }
    else if (line.startsWith("ENABLE,NVE,") || line.startsWith("DISABLE,NVE,")) {
      lines.push(line);
      isNveDefined = true;
    }
    else if (line.length > 0) {
      lines.push(line);
    }
  }
  if (isNveDefined === false) {
    lines.push("DISABLE,NVE,3.");
  }
  iprd01 = lines.join("\n") + "\n";
  productRecords.push(iprd01);
  console.log(iprd01);
})
.then(() => utilities.updateProductRecords(dtc, productRecords))
.then(() => dtc.say("Install dual-state utilities ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[!"
]))
.then(() => dtc.mount(13, 0, 1, "opt/tapes/dual-state.tap"))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Run dual-state.job ..."))
.then(() => dtc.runJob(12, 4, "opt/dual-state.job", [51]))
.then(() => dtc.say("Make new deadstart tape ..."))
.then(() => dtc.dsd([
  "[UNLOAD,50.",
  "[UNLOAD,51.",
  "[!"
]))
.then(() => dtc.sleep(3000))
.then(() => dtc.mount(13, 0, 0, "tapes/ds.tap"))
.then(() => dtc.mount(13, 0, 1, "tapes/dual-state-ds.tap", true))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Run job to write new deadstart tape ..."))
.then(() => dtc.runJob(12, 4, "decks/make-ds-tape.job", [50, 51]))
.then(() => dtc.say("New deadstart tape created: tapes/dual-state-ds.tap"))
.then(() => dtc.disconnect())
.then(() => dtc.exec("node", ["pfdump", dumpTape]))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.shutdown(false))
.then(() => {
  updateIniFile("tapes/CIP870L847.tap");
  return Promise.resolve();
})
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Deadstart system using CIP tape"))
.then(() => dtc.start(["tape"], {
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
}))
.then(() => dtc.sleep(5000))
.then(() => dtc.connect())
.then(() => dtc.expect([{ re: /Operator> $/ }]))
.then(() => dtc.console("idle off"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Install CIP on disk. This takes time, please wait ..."))
.then(() => dtc.dsd([
  "#2000#B!",
  "#1000#I!",
  "#1000#01",
  "#1000#",
  "#1000#"
]))
.then(() => dtc.sleep(90000))
.then(() => dtc.say("CIP installation complete"))
.then(() => {
  dtc.isExitOnClose = false;
  return Promise.resolve();
})
.then(() => dtc.send("shutdown"))
.then(() => dtc.expect([{ re: /Goodbye for now/ }]))
.then(() => {
  return new Promise((resolve, reject) => {
    dtc.shutdownResolver = () => {
      resolve();
    };
  });
})
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Deadstart using CIP on disk"))
.then(() => dtc.start(["manual"], {
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
}))
.then(() => dtc.sleep(5000))
.then(() => dtc.connect())
.then(() => dtc.expect([{ re: /Operator> $/ }]))
.then(() => dtc.console("idle off"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Install NOS/VE boot components ..."))
.then(() => dtc.dsd([
  "#2000#U!",
  "#1000#V!",
  "#1000#",
  "#1000#21",
  "#1000#",
  "#1000#"
]))
.then(() => dtc.sleep(10000))
.then(() => dtc.say("Deadstart NOS 2.8.7 from new tape ..."))
.then(() => dtc.mount(13, 0, 1, "tapes/dual-state-ds.tap"))
.then(() => dtc.sleep(5000))
.then(() => dtc.dsd([
  "#1000#H!",
  "#1000#",
  "#1000#I!",
  "#1000#O!",
  "#1000#P!",
  "#1000#D=YES",
  "#1000#^!",
  "#2000#S!",
  "#1000#T!",
  "#1000#",
  "#1000#",
  "#1000#",
  "#1000#1",
  "#60000#NEXT.",
  "#1000#]!",
  "#1000#INITIALIZE,AL,10,11,12,13.",
  "#1000#GO."
]))
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Install deadstart file on disk ..."))
.then(() => dtc.dis([
  "COMMON,SYSTEM.",
  "INSTALL,SYSTEM,EQ10."
], "INSTALL"))
.then(() => dtc.disconnect())
.then(() => dtc.exec("node", ["pfload", dumpTape]))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Shutdown the system to deadstart restored system using disk ..."))
.then(() => dtc.shutdown(false))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Deadstart NOS 2.8.7 using disk ..."))
.then(() => dtc.start({
  detached: true,
  stdio:    [0, "ignore", 2],
  unref:    false
}))
.then(() => dtc.sleep(5000))
.then(() => dtc.connect())
.then(() => dtc.expect([{ re: /Operator> $/ }]))
.then(() => dtc.console("idle off"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Deadstart complete"))
.then(() => {
  const props = utilities.getCustomProperties(dtc);
  return dtc.say("Create NOS/VE start-up procedures ...")
  .then(() => dtc.dsd(`X.SETVE(,NVE,${utilities.getPropertyValue(props, "PASSWORDS", "NVE", "NVEX")},,F,,0)`))
  .then(() => dtc.sleep(5000))
  .then(() => dtc.dsd(`X.SETVE(WAIT,NVE,${utilities.getPropertyValue(props, "PASSWORDS", "NVE", "NVEX")},,T,,0)`))
  .then(() => dtc.sleep(10000));
})
.then(() => dtc.say(""))
.then(() => dtc.say("--- Upgrade to Cyber 870 complete ---"))
.then(() => dtc.say(""))
.then(() => {
  if (installNVE) {
    let promise = dtc.say("Install NOS/VE ...")
    .then(() => dtc.disconnect());
    if (autoNVE) {
      promise = promise.then(() => dtc.exec("node", ["install-nve"]));
    }
    else {
      promise = promise.then(() => dtc.exec("node", ["install-nve", "-auto"]));
    }
    return promise
    .then(() => dtc.connect())
    .then(() => dtc.expect([ {re:/Operator> $/} ]));
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.say("Enter 'exit' command to exit and terminate system gracefully"))
.then(() => dtc.engageOperator(cmdExtensions))
.then(() => dtc.shutdown())
.then(() => {
  process.exit(0);
})
.catch(err => {
  process.stderr.write(`${err}\n`);
  process.exit(1);
});
