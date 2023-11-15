#!/usr/bin/env node

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");

const dtc = new DtCyber();

let systemRecords = [];    // textual records to edit into PRODUCT file
let props         = {};    // properties read from property file arguments

/*
 * getSystemRecord
 *
 * Obtain a record from the system file of the running system.
 *
 * Arguments:
 *   rid     - record id (e.g., CMRDECK or PROC/XYZZY)
 *   options - optional object providing job credentials and HTTP hostname
 *
 * Returns:
 *   A promise that is resolved when the record has been returned.
 *   The value of the promise is the record contents.
 */
const getSystemRecord = (rid, options) => {
  const job = [
    "$COMMON,SYSTEM.",
    `$GTR,SYSTEM,REC.${rid}`,
    "$REWIND,REC.",
    "$COPYSBF,REC."
  ];
  if (typeof options === "undefined") options = {};
  options.jobname = "GTRSYS";
  return submitJob(job, options);
};

/*
 * processCmrdProps
 *
 * Process properties defined in CMRDECK sections of property files.
 *
 * Returns:
 *  A promise that is resolved when all CMRD properties have been processed.
 *  The global array systemRecords is updated to include the CMRD record
 *  to be edited into the PRODUCT file, if any.
 */
const processCmrdProps = () => {
  if (typeof props["CMRDECK"] !== "undefined") {
    return dtc.say("Edit CMRDECK")
    .then(() => getSystemRecord("CMRDECK"))
    .then(cmrdeck => {
      for (const prop of props["CMRDECK"]) {
        let ei = prop.indexOf("=");
        if (ei < 0) {
          throw new Error(`Invalid CMRDECK definition: \"${prop}\"`);
        }
        let key   = prop.substring(0, ei).trim().toUpperCase();
        let value = prop.substring(ei + 1).trim().toUpperCase();
        let si = 0;
        let isEQyet = false;
        let isPFyet = false;
        while (si < cmrdeck.length) {
          let ni = cmrdeck.indexOf("\n", si);
          if (ni < 0) ni = cmrdeck.length - 1;
          let ei = cmrdeck.indexOf("=", si);
          if (ei < ni && ei > 0) {
            let cmrdKey = cmrdeck.substring(si, ei).trim();
            if (cmrdKey.startsWith("EQ")) {
              isEQyet = true;
            }
            if (cmrdKey === "PF") {
              isPFyet = true;
            }
            if (cmrdKey === key) {
              if (key === "PF") {
                let ci = value.indexOf(",");
                if (ci < 0) {
                  throw new Error(`Invalid CMRDECK definition: \"${prop}\"`);
                }
                let propPFN = parseInt(value.substring(0, ci).trim());
                ci = cmrdeck.indexOf(",", ei + 1);
                let cmrdPFN = parseInt(cmrdeck.substring(ei + 1, ci).trim());
                if (propPFN === cmrdPFN) {
                  cmrdeck = `${cmrdeck.substring(0, si)}${key}=${value}\n${cmrdeck.substring(ni + 1)}`;
                  break;
                }
                else if (propPFN < cmrdPFN) {
                  cmrdeck = `${cmrdeck.substring(0, si)}${key}=${value}\n${cmrdeck.substring(si)}`;
                  break;
                }
              }
              else {
                cmrdeck = `${cmrdeck.substring(0, si)}${key}=${value}\n${cmrdeck.substring(ni + 1)}`;
                break;
              }
            }
            else if (isEQyet && key.startsWith("EQ") && !cmrdKey.startsWith("*")) {
              if (!cmrdKey.startsWith("EQ")
                  || parseInt(key.substring(2)) < parseInt(cmrdKey.substring(2))) {
                cmrdeck = `${cmrdeck.substring(0, si)}${key}=${value}\n${cmrdeck.substring(si)}`;
                break;
              }
            }
            else if (isPFyet && key === "PF" && !cmrdKey.startsWith("*") && cmrdKey !== "REMOVE") {
              cmrdeck = `${cmrdeck.substring(0, si)}${key}=${value}\n${cmrdeck.substring(si)}`;
              break;
            }
          }
          si = ni + 1;
        }
        if (si >= cmrdeck.length) {
          cmrdeck += `${key}=${value}\n`;
        }
      }
      systemRecords.push(cmrdeck);
      return Promise.resolve();
    });
  }
  else {
    return Promise.resolve();
  }
};

/*
 * submitJob
 *
 * Submit a job to the running system.
 *
 * Arguments:
 *   body     - body of the job
 *   options  - optional object providing parameters such as
 *              job nqme, credentials, data, and HTTP hostname
 *
 * Returns:
 *   A promise that is resolved when the job has completed.
 *   The value of the promise is the job output.
 */
const submitJob = (body, options) => {
  let jobname  = "JOB";
  let username = "INSTALL";
  let password = "INSTALL";
  let data     = null;
  if (typeof options === "object") {
    if (typeof options.jobname  === "string") jobname  = options.jobname ;
    if (typeof options.username === "string") username = options.username;
    if (typeof options.password === "string") password = options.password;
    if (typeof options.data     === "string") data     = options.data;
  }
  const beginOutput = ` *** ${jobname} BEGIN OUTPUT ***`;
  const endOutput   = ` *** ${jobname} END OUTPUT ***`;
  if (Array.isArray(body)) body = body.join("\n") + "\n";
  let job = `${jobname}.\n$USER,${username},${password}.\n`
    + "$COPYBR,INPUT,OUTPUT.\n"
    + "$COPYBR,INPUT,ENDOUT.\n"
    + body
    + "$REWIND,ENDOUT.\n"
    + "$COPYBR,ENDOUT,OUTPUT.\n"
    + `$PACK,OUTPUT.\n*** ${jobname} COMPLETE\n`
    + "EXIT.\n$NOEXIT.\n$SKIPEI,OUTPUT.\n"
    + "$REWIND,ENDOUT.\n"
    + "$COPYBR,ENDOUT,OUTPUT.\n"
    + `$PACK,OUTPUT.\n*** ${jobname} FAILED\n`
    + `~eor\n${beginOutput}\n`
    + `~eor\n${endOutput}\n`;
  if (data !== null) job += `~eor\n${data}`;
  fs.writeFileSync("decks/$$$.job", job);
  return new Promise((resolve, reject) => {
    let output = "";
    dtc.runJob(12, 4, "decks/$$$.job", data => {
      output += data;
    })
    .then(() => {
      fs.unlinkSync("decks/$$$.job");
      let bi = output.indexOf(beginOutput);
      if (bi >= 0) {
        output = output.substring(bi + beginOutput.length);
        bi = output.indexOf("\n");
        output = output.substring(bi + 1);
      }
      let ei = output.indexOf(endOutput);
      if (ei >= 0) {
        output = output.substring(0, ei);
      }
      let lines = output.split("\n");
      lines = lines.slice(0, lines.length - 1);
      let result = "";
      for (const line of lines) {
        result += line.substring(1) + "\n";
      }
      resolve(result);
    });
  });
};

/*
 * updateSystemRecords
 *
 * Update the SYSTEM file to include any new or modified records that have
 * been defined.
 *
 * Returns:
 *  A promise that is resolved when the PRODUCT file has been updated.
 */
const updateSystemRecords = () => {
  if (systemRecords.length > 0) {
    const job = [
      "$SETTL,7777.",
      "$ATTACH,SYSTEM/M=W.",
      "$COPY,INPUT,LGO.",
      "$RFL,100000.",
      "$LIBEDIT,P=SYSTEM,B=LGO,I=0,L=0,C."
    ];
    const options = {
      jobname: "UPDPSYS",
      data:    `${systemRecords.join("~eor\n")}`
    };
    return dtc.say("Update SYSTEM file")
    .then(() => submitJob(job, options))
    .then(output => {
      return Promise.resolve();
    });
  }
  else {
    return Promise.resolve();
  }
};

//
//  Read the property files, if any, and build the property object from them
//

let propFiles = [];
for (const filePath of process.argv.slice(2)) {
  propFiles.push(filePath);
}
if (propFiles.length < 1) {
  if (fs.existsSync("site.cfg")) {
    propFiles.push("site.cfg");
  }
  else {
    process.exit(0); // No property files specified
  }
}

for (const configFilePath of propFiles) {
  if (!fs.existsSync(configFilePath)) {
    process.stdout.write(`${configFilePath} does not exist\n`);
    process.exit(1);
  }
  const lines = fs.readFileSync(configFilePath, "utf8");
  let sectionKey = "";

  for (let line of lines.split("\n")) {
    line = line.trim();
    if (line.length < 1 || /^[;#*]/.test(line)) continue;
    if (line.startsWith("[")) {
      let bi = line.indexOf("]");
      if (bi < 0) {
        process.stderr.write(`Invalid section key in ${configFilePath}: \"${line}\"\n`);
        process.exit(1);
      }
      sectionKey = line.substring(1, bi).trim().toUpperCase();
    }
    else if (sectionKey !== "") {
      if (typeof props[sectionKey] === "undefined") {
        props[sectionKey] = [];
      }
      props[sectionKey].push(line);
    }
    else {
      process.stderr.write(`Property defined before first section key: \"${line}\"\n`);
      process.exit(1);
    }
  }
}

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E6"))
.then(() => dtc.console("idle off"))
.then(() => processCmrdProps())
.then(() => updateSystemRecords())
.then(() => dtc.say("Reconfiguration complete"))
.then(() => dtc.console("idle on"))
.then(() => {
  process.exit(0);
});
