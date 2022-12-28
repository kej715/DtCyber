#!/usr/bin/env node

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const http      = require("http");
const net       = require("net");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

let newMID         = null;  // new machine identifer
let oldMID         = null;  // old machine identifer
let productRecords = [];    // textual records to edit into PRODUCT file
let props          = {};    // properties read from property file arguments

/*
 * asciiToCdc
 *
 * Translate ASCII to CDC 6/12 display code.
 *
 * Arguments:
 *   ascii - the ASCII to convert
 *
 * Returns:
 *   CDC 6/12 display code
 */
const asciiToCdc = ascii => {
  let result = "";
  let i = 0;

  while (i < ascii.length) {
    let c = ascii.charAt(i++);
    switch (c) {
    case "a": result += "^A"; break;
    case "b": result += "^B"; break;
    case "c": result += "^C"; break;
    case "d": result += "^D"; break;
    case "e": result += "^E"; break;
    case "f": result += "^F"; break;
    case "g": result += "^G"; break;
    case "h": result += "^H"; break;
    case "i": result += "^I"; break;
    case "j": result += "^J"; break;
    case "k": result += "^K"; break;
    case "l": result += "^L"; break;
    case "m": result += "^M"; break;
    case "n": result += "^N"; break;
    case "o": result += "^O"; break;
    case "p": result += "^P"; break;
    case "q": result += "^Q"; break;
    case "r": result += "^R"; break;
    case "s": result += "^S"; break;
    case "t": result += "^T"; break;
    case "u": result += "^U"; break;
    case "v": result += "^V"; break;
    case "w": result += "^W"; break;
    case "x": result += "^X"; break;
    case "y": result += "^Y"; break;
    case "z": result += "^Z"; break;
    case "{": result += "^0"; break;
    case "|": result += "^1"; break;
    case "}": result += "^2"; break;
    case "~": result += "^3"; break;
    case "@": result += "@A"; break;
    case "^": result += "@B"; break;
    case ":": result += "@D"; break;
    case "`": result += "@G"; break;
    default:  result += c   ; break;
    }
  }
  return result;
};

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
 * cdcToAscii 
 *
 * Translate CDC 6/12 display code to ASCII.
 *
 * Arguments:
 *   displayCode - the display code to convert
 *
 * Returns:
 *   ASCII result
 */
const cdcToAscii = displayCode => {
  let result = "";
  let i = 0;

  while (i < displayCode.length) {
    let c = displayCode.charAt(i++);
    if (c === "^" && i < displayCode.length) {
      c = displayCode.charAt(i++);
      switch (c) {
      case "A": result += "a"; break;
      case "B": result += "b"; break;
      case "C": result += "c"; break;
      case "D": result += "d"; break;
      case "E": result += "e"; break;
      case "F": result += "f"; break;
      case "G": result += "g"; break;
      case "H": result += "h"; break;
      case "I": result += "i"; break;
      case "J": result += "j"; break;
      case "K": result += "k"; break;
      case "L": result += "l"; break;
      case "M": result += "m"; break;
      case "N": result += "n"; break;
      case "O": result += "o"; break;
      case "P": result += "p"; break;
      case "Q": result += "q"; break;
      case "R": result += "r"; break;
      case "S": result += "s"; break;
      case "T": result += "t"; break;
      case "U": result += "u"; break;
      case "V": result += "v"; break;
      case "W": result += "w"; break;
      case "X": result += "x"; break;
      case "Y": result += "y"; break;
      case "Z": result += "z"; break;
      case "0": result += "{"; break;
      case "1": result += "|"; break;
      case "2": result += "}"; break;
      case "3": result += "~"; break;
      default:
        result += "^";
        i -= 1;
        break;
      }
    }
    else if (c === "@" && i < displayCode.length) {
      c = displayCode.charAt(i++);
      switch (c) {
      case "A": result += "@"; break;
      case "B": result += "^"; break;
      case "D": result += ":"; break;
      case "G": result += "`"; break;
      default:
        result += "@";
        i -= 1;
        break;
      }
    }
    else {
      result += c;
    }
  }
  return result;
};

/*
 * findProperty
 *
 * Find a property in a section of the configuration object.
 *
 * Arguments:
 *   sectionKey - the name of the section to search
 *   propName   - the name of the property to find
 *
 * Result:
 *   the property value, or null if the property is not found
 */
const findProperty = (sectionKey, propName) => {
  propName = propName.toUpperCase();
  if (typeof props[sectionKey] === "undefined") return null;
  for (const prop of props[sectionKey]) {
    let ei = prop.indexOf("=");
    if (ei >= 0 && propName === prop.substring(0, ei).trim().toUpperCase()) {
      return prop.substring(ei + 1).trim();
    }
  }
  return null;
};

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
    "$COPY,FILE."
  ];
  if (typeof options === "undefined") options = {};
  options.jobname = "GETFILE";
  return submitJob(job, options);
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
    `$GTR,SYSTEM,OUTPUT.${rid}`,
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
    `$REPLACE,FILE=${filename}.`,
  ];
  if (typeof options === "undefined") options = {};
  options.jobname = "REPFILE";
  options.data    = data;
  return submitJob(job, options);
};

/*
 * submitJob
 *
 * Submit a job to the running system.
 *
 * Arguments:
 *   body    - body of the job
 *   options - optional object providing parameters such as
 *             job nqme, credentials, data, and HTTP hostname
 *
 * Returns:
 *   A promise that is resolved when the job has completed.
 *   The value of the promise is the job output.
 */
const submitJob = (body, options) => {
  let jobname  = "JOB";
  let username = "INSTALL";
  let password = "INSTALL";
  let hostname = "127.0.0.1";
  let data     = null;
  if (typeof options === "object") {
    if (typeof options.jobname  === "string") jobname  = options.jobname ;
    if (typeof options.username === "string") username = options.username;
    if (typeof options.password === "string") password = options.password;
    if (typeof options.hostname === "string") hostname = options.hostname;
    if (typeof options.data     === "string") data     = options.data;
  }
  if (Array.isArray(body)) body = body.join("\n") + "\n";
  body += `$EXIT.\n*** ${jobname} FAILED\n`;
  if (data !== null) body += `~eor\n${data}`;
  return dtc.postJob(`${jobname}.\n$USER,${username},${password}.\n${body}`, hostname);
};

/*
 * updateLIDConf
 *
 * Update the LID configuration file to include new/updated LID definitions, if any.
 *
 * Returns:
 *  A promise that is resolved when the LID configuration file has been updated.
 */
const updateLIDConf = () => {
  if (typeof props["LIDS"] !== "undefined") {
    let promise = Promise.resolve();
    let mid = newMID;
    if (mid === null) {
      promise = getSystemRecord("CMRD01")
      .then(cmrd01 => {
        for (const defn of cmrd01.split("\n")) {
          if (defn.trim().startsWith("MID")) {
            const tokens = defn.split("=");
            if (tokens.length > 1) {
              mid = tokens[1].trim();
              return Promise.resolve();
            }
          }
        }
        mid = "01";
        return Promise.resolve();
      });
    }
    return promise
    .then(() => dtc.say(`Update LIDCM${mid}`))
    .then(() => {
      //
      // Build an object representing the contents of the LIDCMxx file.
      // Start with the PID and LID definition of the local host.
      //
      let lidConf = {};
      let currentPid = `M${mid}`;
      lidConf[currentPid] = {
        pid: {
          PID: currentPid,
          MFTYPE: "NOS2"
        },
        lids: {}
      };
      lidConf[currentPid].lids[currentPid] = {
        LID: currentPid
      };
      //
      // Update the object to include PID's and LID's in the LIDS property.
      //
      for (const defn of props["LIDS"]) {
        let tokens = defn.trim().toUpperCase().split(/[,.]/);
        if (tokens.length < 2) continue;
        let attrs = {};
        for (const attr of tokens.slice(1)) {
          const ei = attr.indexOf("=");
          if (ei > 0) {
            attrs[attr.substring(0, ei).trim()] = attr.substring(ei + 1).trim();
          }
        }
        if (tokens[0] === "NPID" && typeof attrs["PID"] !== "undefined") {
          currentPid = attrs["PID"];
          lidConf[currentPid] = {
            pid: attrs,
            lids: {}
          };
        }
        else if (tokens[0] === "NLID" && currentPid !== null && typeof attrs["LID"] !== "undefined") {
          lidConf[currentPid].lids[attrs["LID"]] = attrs;
        }
      }
      //
      // Generate new contents of the LIDCMxx file from the updated object.
      //
      let lidText = `LIDCM${mid}\n`;
      for (const pid of Object.keys(lidConf).sort()) {
        let pidAttrs = lidConf[pid].pid;
        lidText += `NPID,PID=${pid}`;
        for (const pidAttrKey of Object.keys(pidAttrs)) {
          if (pidAttrKey !== "PID") {
            lidText += `,${pidAttrKey}=${pidAttrs[pidAttrKey]}`;
          }
        }
        lidText += ".\n";
        let lids = lidConf[pid].lids;
        for (const lid of Object.keys(lids)) {
          lidText += `NLID,LID=${lid}`;
          for (const lidAttrKey of Object.keys(lids[lid])) {
            if (lidAttrKey !== "LID") {
              lidText += `,${lidAttrKey}=${lids[lid][lidAttrKey]}`;
            }
          }
          lidText += ".\n";
        }
      }
      return lidText;
    })
    .then(text => replaceFile(`LIDCM${mid}`, text))
    .then(() => utilities.moveFile(dtc, `LIDCM${mid}`, 1, 377777));
  }
  else {
    return Promise.resolve();
  }
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
      return submitJob(job, options);
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
    .then(() => submitJob(job, options))
    .then(output => {
      for (const line of output.split("\n")) {
        console.log(`${new Date().toLocaleTimeString()}   ${line.substring(1)}`);
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
  if (oldMID !== newMID || typeof props["HOSTS"] !== "undefined") {
    return dtc.say("Update TCPHOST")
    .then(() => getFile("TCPHOST", {username:"NETADMN",password:"NETADMN"}))
    .then(text => {
      text = cdcToAscii(text);
      if (oldMID !== newMID) {
        const regex = new RegExp(`LOCALHOST_${oldMID}`, "i");
        while (true) {
          let si = text.search(regex);
          if (si < 0) break;
          text = `${text.substring(0, si)}LOCALHOST_${newMID}${text.substring(si + 12)}`;
        }
      }
      const pid = `M${oldMID}`;
      let hosts = {};
      for (const line of text.split("\n")) {
        if (/^[0-9]/.test(line)) {
          const tokens = line.split(/\s+/);
          if (tokens.length < 2) continue;
          for (let i = 1; i < tokens.length; i++) {
            if (tokens[i].toUpperCase() === pid) {
              tokens[i] = `M${newMID}`;
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
      return asciiToCdc(text);
    })
    .then(text => replaceFile("TCPHOST", text, {username:"NETADMN",password:"NETADMN"}));
  }
  else {
    return Promise.resolve();
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
    .then(text => replaceFile("TCPRSLV", asciiToCdc(`${props["RESOLVER"].join("\n")}\n`), {username:"NETADMN",password:"NETADMN"}))
    .then(() => submitJob(job, options));
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

awaitService(80, 60)
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => processCmrdProps())
.then(() => processEqpdProps())
.then(() => updateProductRecords())
.then(() => updateMachineID())
.then(() => updateLIDConf())
.then(() => updateTcpHosts())
.then(() => updateTcpResolver())
.then(() => dtc.say("Reconfiguration complete"))
.then(() => {
  process.exit(0);
});
