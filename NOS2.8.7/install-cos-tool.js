#!/usr/bin/env node
//
// This script re/installs one or more tools from the cos-tools tape.
//
const fs        = require("fs");
const DtCyber   = require("../automation/DtCyber");
const utilities = require("./opt/utilities");

const usage = () => {
  process.stderr.write("Usage: node install-cos-tool <name> ...]\n");
  process.stderr.write("  <name>   name of tool to install\n");
  process.exit(1);
};

if (process.argv.length < 3) usage();

const dtc = new DtCyber();

const libraries = [ "CLIB","EMLIB","INTFLIB","IOLIB","RTLIB","SYSLIB" ];
const extDiskBins = [ "AUDIT" ];

let job = [
  "$NOEXIT.",
  "$TMSDEF,TO=F.",
  "$ONEXIT.",
  "$ATTACH,COSTLDB/NA.",
  "$IF,.NOT.FILE(COSTLDB,AS),CREATE.",
  "  $RECLAIM,DB=COSTLDB,Z./LIST,TN=COSTLS",
  "$ENDIF,CREATE."
];
let tools = process.argv.slice(2);
let tapeFiles = [];
for (let tool of tools) {
  tool = tool.toUpperCase();
  if (extDiskBins.indexOf(tool) < 0) {
    tapeFiles.push(tool);
  }
}
if (tapeFiles.length > 0) {
  job.push(`$RECLAIM,DB=COSTLDB,Z./LOAD,TN=COSTLS,RP=Y,PF=*/${tapeFiles.join(",")}`);
}
for (let tool of tools) {
  tool = tool.toUpperCase();
  if (libraries.indexOf(tool) >= 0) {
    job.push(`$BEGIN,REPLACE,CRAY,${tool},DC=BC.`);
  }
  else if (extDiskBins.indexOf(tool) >= 0) {
    job.push(`$BEGIN,INSTALL,CRAY,${tool},DC=BC,EXT=YES.`);
  }
  else {
    job.push(`$BEGIN,INSTALL,CRAY,${tool},DC=BC.`);
  }
}
const options = {
  jobname:  "COSTOOL",
  username: "INSTALL",
  password: utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "INSTALL", "INSTALL")
};

let url = null;

const products = JSON.parse(fs.readFileSync("opt/products.json", "utf8"));
for (const category of products) {
  if (category.category === "datacomm") {
    for (product of category.products) {
      if (product.name === "cos-tools") {
        url = product.url;
      }
    }
  }
}
if (url === null) {
  process.stderr.write("URL of cos-tools tape not found\n");
  process.exit(1);
}

let progressMaxLen = 0;

let promise = dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Download cos-tools tape image ..."))
.then(() => dtc.wget(url, "opt/tapes", "cos-tools.tap", (byteCount, contentLength) => {
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
}))
.then(() => dtc.say("Mount tape ..."))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[!"
]))
.then(() => dtc.mount(13, 0, 1, "opt/tapes/cos-tools.tap"))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Run job to initiate installation ..."))
.then(() => dtc.createJobWithOutput(12, 4, job, options))
.then(() => dtc.say("Installation initiated, check printer for job output from COS"))
.then(() => dtc.dsd([
  "[UNLOAD,51.",
  "[!"
]))
.then(() => {
  process.exit(0);
})
.catch(err => {
  process.stderr.write(`${err}\n`);
  process.exit(1);
});
