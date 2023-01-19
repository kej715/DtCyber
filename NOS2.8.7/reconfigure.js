#!/usr/bin/env node

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const http      = require("http");
const net       = require("net");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

let newHostID      = null;  // new network host identifier
let newMID         = null;  // new machine identifer
let oldHostID      = null;  // old network host identifier
let oldMID         = null;  // old machine identifer
let productRecords = [];    // textual records to edit into PRODUCT file
let props          = {};    // properties read from property file arguments

/*
 * awaitService
 *
 * Wait for a TCP service to become available.
 *
 * Arguments:
 *   port        - the port on which to wait
 *   maxWaitTime - maximum number of seconds to wait
 *
 * Returns:
 *   A promise that is fulfilled when the service is available,
 *   or rejected if the maximum wait time is exceeded.
 */
const awaitService = (port, maxWaitTime) => {
  const deadline = Date.now() + (maxWaitTime * 1000);
  let   attempts = 0;
  return new Promise((resolve, reject) => {
    const testService = () => {
      if (Date.now() > deadline) {
        reject(new Error(`Service at port ${port} is unavailable`));
      }
      else {
        try {
          attempts += 1;
          if (attempts > 1) {
            process.stdout.write(`\r${new Date().toLocaleTimeString()} Attempt ${attempts} : connecting to port ${port}\r`);
          }
          const socket = net.createConnection({port:port, host:"127.0.0.1"}, () => {
            socket.end();
            resolve();
          });
          socket.on("error", err => {
            setTimeout(() => { testService(); }, 5000);
          });
        }
        catch (err) {
          setTimeout(() => { testService(); }, 5000);
        }
      }
    };
    testService();
  });
}

/*
 * getFile
 *
 * Obtain a file from the running system.
 *
 * Arguments:
 *   filename - file name (e.g., LIDCM01/UN=SYSTEMX)
 *   options  - optional object providing job credentials and HTTP hostname
 *
 * Returns:
 *   A promise that is resolved when the file contents have been returned.
 *   The value of the promise is the file contents.
 */
const getFile = (filename, options) => {
  const job = [
    `$GET,FILE=${filename}.`,
    "$COPYSBF,FILE."
  ];
  if (typeof options === "undefined") options = {};
  options.jobname = "GETFILE";
  return dtc.createJobWithOutput(12, 4, job, options);
};

/*
 * getSystemRecord
 *
 * Obtain a record from the system file of the running system.
 *
 * Arguments:
 *   rid     - record id (e.g., CMRD01 or PROC/XYZZY)
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
  return dtc.createJobWithOutput(12, 4, job, options);
};

/*
 * isInstalled
 *
 * Determine whether a specified produce has been installed.
 *
 * Arguments:
 *   product - name of product
 *
 * Returns:
 *   TRUE if product is installed
 */
const isInstalled = product => {
  if (fs.existsSync("opt/installed.json")) {
    const installedProductSet = JSON.parse(fs.readFileSync("opt/installed.json", "utf8"));
    return installedProductSet.indexOf(product) >= 0;
  }
  return false;
};

/*
 * processCmrdProps
 *
 * Process properties defined in CMRDECK sections of property files.
 *
 * Returns:
 *  A promise that is resolved when all CMRD properties have been processed.
 *  The global array productRecords is updated to include the CMRD record
 *  to be edited into the PRODUCT file, if any.
 */
const processCmrdProps = () => {
  if (typeof props["CMRDECK"] !== "undefined") {
    return dtc.say("Edit CMRD01")
    .then(() => getSystemRecord("CMRD01"))
    .then(cmrd01 => {
      for (const prop of props["CMRDECK"]) {
        let ei = prop.indexOf("=");
        if (ei < 0) {
          throw new Error(`Invalid CMRDECK definition: \"${prop}\"`);
        }
        let key   = prop.substring(0, ei).trim().toUpperCase();
        let value = prop.substring(ei + 1).trim().toUpperCase();
        if (value.endsWith(".")) value = value.substring(0, value.length - 1).trim();
        let si = 0;
        while (si < cmrd01.length) {
          let ni = cmrd01.indexOf("\n", si);
          if (ni < 0) ni = cmrd01.length - 1;
          let ei = cmrd01.indexOf("=", si);
          if (ei < ni && ei > 0 && cmrd01.substring(si, ei).trim() === key) {
            if (key === "MID") {
              newMID = value;
              oldMID = cmrd01.substring(ei + 1, ni).trim();
              if (oldMID.endsWith(".")) oldMID = oldMID.substring(0, oldMID.length - 1).trim();
            }
            cmrd01 = `${cmrd01.substring(0, si)}${key}=${value}.\n${cmrd01.substring(ni + 1)}`;
            break;
          }
          si = ni + 1;
        }
        if (si >= cmrd01.length) {
          cmrd01 += `${key}=${value}\n`;
        }
      }
      productRecords.push(cmrd01);
      return Promise.resolve();
    });
  }
  else {
    return Promise.resolve();
  }
};

/*
 * processEqpdProps
 *
 * Process properties defined in EQPDECK sections of property files.
 *
 * Returns:
 *  A promise that is resolved when all EQPD properties have been processed.
 *  The global array productRecords is updated to include the EQPD record
 *  to be edited into the PRODUCT file, if any.
 */
const processEqpdProps = () => {
  if (typeof props["EQPDECK"] !== "undefined") {
    return dtc.say("Edit EQPD01")
    .then(() => getSystemRecord("EQPD01"))
    .then(eqpd01 => {
      for (const prop of props["EQPDECK"]) {
        let ei = prop.indexOf("=");
        if (ei < 0) {
          throw new Error(`Invalid EQPDECK definition: \"${prop}\"`);
        }
        let key   = prop.substring(0, ei).trim().toUpperCase();
        let value = prop.substring(ei + 1).trim().toUpperCase();
        let si = 0;
        let isEQyet = false;
        let isPFyet = false;
        while (si < eqpd01.length) {
          let ni = eqpd01.indexOf("\n", si);
          if (ni < 0) ni = eqpd01.length - 1;
          let ei = eqpd01.indexOf("=", si);
          if (ei < ni && ei > 0) {
            let eqpdKey = eqpd01.substring(si, ei).trim();
            if (eqpdKey.startsWith("EQ")) {
              isEQyet = true;
            }
            if (eqpdKey === "PF") {
              isPFyet = true;
            }
            if (eqpdKey === key) {
              if (key === "PF") {
                let ci = value.indexOf(",");
                if (ci < 0) {
                  throw new Error(`Invalid EQPDECK definition: \"${prop}\"`);
                }
                let propPFN = parseInt(value.substring(0, ci).trim());
                ci = eqpd01.indexOf(",", ei + 1);
                let eqpdPFN = parseInt(eqpd01.substring(ei + 1, ci).trim());
                if (propPFN === eqpdPFN) {
                  eqpd01 = `${eqpd01.substring(0, si)}${key}=${value}\n${eqpd01.substring(ni + 1)}`;
                  break;
                }
                else if (propPFN < eqpdPFN) {
                  eqpd01 = `${eqpd01.substring(0, si)}${key}=${value}\n${eqpd01.substring(si)}`;
                  break;
                }
              }
              else {
                eqpd01 = `${eqpd01.substring(0, si)}${key}=${value}\n${eqpd01.substring(ni + 1)}`;
                break;
              }
            }
            else if (isEQyet && key.startsWith("EQ") && !eqpdKey.startsWith("*")) {
              if (!eqpdKey.startsWith("EQ")
                  || parseInt(key.substring(2)) < parseInt(eqpdKey.substring(2))) {
                eqpd01 = `${eqpd01.substring(0, si)}${key}=${value}\n${eqpd01.substring(si)}`;
                break;
              }
            }
            else if (isPFyet && key === "PF" && !eqpdKey.startsWith("*") && eqpdKey !== "REMOVE") {
              eqpd01 = `${eqpd01.substring(0, si)}${key}=${value}\n${eqpd01.substring(si)}`;
              break;
            }
          }
          si = ni + 1;
        }
        if (si >= eqpd01.length) {
          eqpd01 += `${key}=${value}\n`;
        }
      }
      productRecords.push(eqpd01);
      return Promise.resolve();
    });
  }
  else {
    return Promise.resolve();
  }
};

/*
 * processNetworkProps
 *
 * Process properties defined in NETWORK sections of property files.
 *
 * Returns:
 *  A promise that is resolved when all NETWORK properties have been processed.
 */
const processNetworkProps = () => {
  let iniProps = {};
  dtc.readPropertyFile("cyber.ini", iniProps);
  if (fs.existsSync("cyber.ovl")) {
    dtc.readPropertyFile("cyber.ovl", iniProps);
  }
  for (let line of iniProps["npu.nos287"]) {
    let ei = line.indexOf("=");
    if (ei < 0) continue;
    let key   = line.substring(0, ei).trim();
    let value = line.substring(ei + 1).trim();
    if (key.toUpperCase() === "HOSTID") {
      oldHostID = line.substring(ei + 1).trim();
    }
  }
  if (typeof props["NETWORK"] !== "undefined") {
    for (const prop of props["NETWORK"]) {
      let ei = prop.indexOf("=");
      if (ei < 0) {
        throw new Error(`Invalid NETWORK definition: \"${prop}\"`);
      }
      let key   = prop.substring(0, ei).trim();
      if (key.toUpperCase() === "HOSTID") {
        newHostID = prop.substring(ei + 1).trim();
      }
    }
  }
  return Promise.resolve();
};

/*
 * replaceFile
 *
 * Replace a file on the running system.
 *
 * Arguments:
 *   filename - file name (e.g., LIDCMXY)
 *   data     - contents of the file
 *   options  - optional object providing job credentials and HTTP hostname
 *
 * Returns:
 *   A promise that is resolved when the file has been replaced.
 */
const replaceFile = (filename, data, options) => {
  const job = [
    `$COPY,INPUT,FILE.`,
    `$REPLACE,FILE=${filename}.`
  ];
  if (typeof options === "undefined") options = {};
  options.jobname = "REPFILE";
  options.data    = data;
  return dtc.createJobWithOutput(12, 4, job, options);
};

/*
 * updateMachineID
 *
 * Create/Update the LIDCMxx file and modify the NDL configuration to reflect the machine's
 * new identifier, if any.
 *
 * Returns:
 *  A promise that is resolved when the machine identifier has been updated.
 */
const updateMachineID = () => {
  if (oldMID !== newMID) {
    return dtc.say(`Create LIDCM${newMID}`)
    .then(() => getFile(`LIDCM${oldMID}/UN=SYSTEMX`))
    .then(text => {
      text = text.replace(`LIDCM${oldMID}`, `LIDCM${newMID}`);
      regex = new RegExp(`[LP]ID=M${oldMID}[,.]`);
      while (true) {
        let si = text.search(regex);
        if (si < 0) break;
        text = `${text.substring(0, si + 5)}${newMID}${text.substring(si + 7)}`;
      }
      return text;
    })
    .then(text => replaceFile(`LIDCM${newMID}`, text))
    .then(() => utilities.moveFile(dtc, `LIDCM${newMID}`, 1, 377777))
    .then(() => dtc.say("Modify TCP/IP gateway OUTCALL statement in NDL ..."))
    .then(() => {
      const data = [
        "TCPGWID",
        "*IDENT TCPGWID",
        "*DECK NETCFG",
        "*D 181,182",
        `OUTCALL,NAME1=TCPIPGW,NAME2=H${newMID},SNODE=1,DNODE=255,NETOSD=DDV,`,
        `        ABL=7,DBL=7,DBZ=2000,UBL=7,UBZ=18,SERVICE=GW_TCPIP_M${newMID}.`,
        "*EDIT NETCFG"
      ];
      const job = [
        "$GET,NDLMODS/NA.",
        "$IF,FILE(NDLMODS,AS),EDIT.",
        "$  COPY,INPUT,MOD.",
        "$  REWIND,MOD.",
        "$  LIBEDIT,P=NDLMODS,B=MOD,I=0,C.",
        "$ELSE,EDIT.",
        "$  COPY,INPUT,NDLMODS.",
        "$ENDIF,EDIT.",
        "$REPLACE,NDLMODS."
      ];
      const options = {
        jobname:  "TCPGWID",
        username: "NETADMN",
        password: "NETADMN",
        data:     `${data.join("\n")}\n`
      };
      return dtc.createJobWithOutput(12, 4, job, options);
    })
    .then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job"));
  }
  else {
    return Promise.resolve();
  }
};

/*
 * updateProductRecords
 *
 * Update the PRODUCT file to include any new or modified records that have
 * been defined.
 *
 * Returns:
 *  A promise that is resolved when the PRODUCT file has been updated.
 */
const updateProductRecords = () => {
  if (productRecords.length > 0) {
    const job = [
      "$SETTL,*.",
      "$SETJSL,*.",
      "$SETASL,*.",
      "$ATTACH,PRODUCT/M=W,WB.",
      "$COPY,INPUT,LGO.",
      "$LIBEDIT,P=PRODUCT,B=LGO,I=0,LO=EM,C."
    ];
    const options = {
      jobname: "UPDPROD",
      data:    `${productRecords.join("~eor\n")}`
    };
    return dtc.say("Update PRODUCT")
    .then(() => dtc.createJobWithOutput(12, 4, job, options))
    .then(output => {
      for (const line of output.split("\n")) {
        console.log(`${new Date().toLocaleTimeString()}   ${line}`);
      }
      return Promise.resolve();
    });
  }
  else {
    return Promise.resolve();
  }
};

/*
 * updateTcpHosts
 *
 * Update the TCPHOST file to reference the local machine ID and to include any
 * additional hosts defined by the HOSTS property, if any.
 *
 * Returns:
 *  A promise that is resolved when the TCPHOST file has been updated.
 */
const updateTcpHosts = () => {

  if (oldMID === newMID && oldHostID === newHostID && typeof props["HOSTS"] === "undefined") {
    return Promise.resolve();
  }
  else {
    return dtc.say("Update TCPHOST")
    .then(() => getFile("TCPHOST", {username:"NETADMN",password:"NETADMN"}))
    .then(text => {
      let hosts = {};
      let pid = `M${oldMID.toUpperCase()}`;
      let hid = oldHostID.toUpperCase();
      let lcl = `LOCALHOST_${oldMID.toUpperCase()}`;
      text = dtc.cdcToAscii(text);
      for (const line of text.split("\n")) {
        if (/^[0-9]/.test(line)) {
          const tokens = line.split(/\s+/);
          if (tokens.length < 2) continue;
          for (let i = 1; i < tokens.length; i++) {
            let token = tokens[i].toUpperCase();
            if (token === pid && newMID !== null) {
              tokens[i] = `M${newMID}`;
            }
            else if (token === hid && newHostID !== null) {
              tokens[i] = newHostID;
            }
            else if (token === lcl && newMID !== null) {
              tokens[i] = `LOCALHOST_${newMID}`;
            }
          }
          hosts[tokens[1].toUpperCase()] = tokens.join(" ");
        }
      }
      if (typeof props["HOSTS"] !== "undefined") {
        for (const defn of props["HOSTS"]) {
          if (/^[0-9]/.test(defn)) {
            const tokens = defn.split(/\s+/);
            if (tokens.length > 1) {
              hosts[tokens[1].toUpperCase()] = tokens.join(" ");
            }
          }
        }
      }
      text = "";
      for (const key of Object.keys(hosts).sort()) {
        text += `${hosts[key]}\n`;
      }
      return dtc.asciiToCdc(text);
    })
    .then(text => replaceFile("TCPHOST", text, {username:"NETADMN",password:"NETADMN"}));
  }
};

/*
 * updateTcpResolver
 *
 * If a RESOLVER property is defined, create/update the TCPRSLV file to reflect the TCP/IP
 * resource resolver defined by it.
 *
 * Returns:
 *  A promise that is resolved when the TCPHOST file has been updated.
 */
const updateTcpResolver = () => {
  if (typeof props["RESOLVER"] !== "undefined") {
    const job = [
      "$CHANGE,TCPRSLV/CT=PU,M=R,AC=Y."
    ];
    const options = {
      jobname: "MAKEPUB",
      username: "NETADMN",
      password: "NETADMN"
    };
    return dtc.say("Create/Update TCPRSLV")
    .then(text => replaceFile("TCPRSLV", dtc.asciiToCdc(`${props["RESOLVER"].join("\n")}\n`), {username:"NETADMN",password:"NETADMN"}))
    .then(() => dtc.createJobWithOutput(12, 4, job, options));
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
  dtc.readPropertyFile(configFilePath, props);
}

awaitService(80, 60)
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => processCmrdProps())
.then(() => processEqpdProps())
.then(() => processNetworkProps())
.then(() => updateProductRecords())
.then(() => updateMachineID())
.then(() => updateTcpHosts())
.then(() => updateTcpResolver())
.then(() => dtc.disconnect())
.then(() => {
  return isInstalled("njf") ? dtc.exec("node", ["njf-configure"]) : Promise.resolve();
})
.then(() => {
  return isInstalled("mailer") ? dtc.exec("node", ["mailer-configure"]) : Promise.resolve();
})
.then(() => dtc.say("Reconfiguration complete"))
.then(() => {
  process.exit(0);
});
