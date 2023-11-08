#!/usr/bin/env node

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const os        = require("os");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

let emEqOrdinal      = 6;           // ESM equipment ordinal
let newHostID        = null;        // new network host identifier
let newMID           = null;        // new machine identifer
let oldHostID        = "NCCM01";    // old network host identifier
let oldMID           = "01";        // old machine identifer
let productRecords   = [];          // textual records to edit into PRODUCT file
let updatedPasswords = [];          // list of passwords that have been updated

const customProps    = utilities.getCustomProperties(dtc);
const iniProps       = dtc.getIniProperties();
let   ovlProps       = {};
let   newIpAddress   = "127.0.0.1";
let   passwordMap    = JSON.parse(fs.readFileSync("opt/password-map.json", "utf8"));

let oldCrsInfo = {
  lid:       "COS",
  channel:   -1,
  stationId: "FE",
  crayId:    "C1"
};
let newCrsInfo = {};

const tapMgrs = {
  Darwin:     "../ifcmgrs/tapmgr.macos",
  Linux:      "../ifcmgrs/tapmgr.linux",
  Windows_NT: "../ifcmgrs/tapmgr.bat"
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
  if (typeof customProps["CMRDECK"] !== "undefined") {
    return dtc.say("Edit CMRD01 ...")
    .then(() => utilities.getSystemRecord(dtc, "CMRD01"))
    .then(cmrd01 => {
      for (const prop of customProps["CMRDECK"]) {
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
  if (typeof customProps["EQPDECK"] !== "undefined"
      || (oldMID !== newMID && newMID !== null)) {
    return dtc.say("Edit EQPD01 ...")
    .then(() => utilities.getSystemRecord(dtc, "EQPD01"))
    .then(eqpd01 => {
      if (oldMID !== newMID && newMID !== null) {
        //
        //  Check for XM entry in existing equipment deck
        //
        let updatedDeck = [];
        for (const line of eqpd01.split("\n")) {
          if (line.startsWith("XM=")) {
            let fields = line.split(",");
            updatedDeck.push(`XM=${newMID},${fields.slice(1).join(",")}`);
          }
          else {
            updatedDeck.push(line);
          }
        }
        eqpd01 = updatedDeck.join("\n");
      }
      //
      //  Edit equipment deck to include new/updated entries
      //
      
      if (typeof customProps["EQPDECK"] !== "undefined") {
        for (const prop of customProps["EQPDECK"]) {
          let ei = prop.indexOf("=");
          if (ei < 0) {
            ei = prop.indexOf(",");
            if (ei < 0) {
              throw new Error(`Invalid EQPDECK definition: \"${prop}\"`);
            }
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
              else if (eqpdKey === "PF") {
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
            eqpd01 += `${prop.toUpperCase()}\n`;
          }
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
  oldHostID = utilities.getPropertyValue(iniProps,    "npu.nos287", "hostID", oldHostID);
  newHostID = utilities.getPropertyValue(customProps, "NETWORK",    "hostID", newHostID);
  let value = utilities.getPropertyValue(iniProps,    "sysinfo",    "CRS",    null);
  if (value !== null) {
    let items = value.split(",");
    oldCrsInfo.lid       = items[0];
    oldCrsInfo.channel   = parseInt(items[1], 8);
    oldCrsInfo.stationId = items[2];
    oldCrsInfo.crayId    = items[3];
  }
  value = utilities.getPropertyValue(customProps, "NETWORK", "crayStation", null);
  if (value !== null) {
    //
    //  crayStation=<name>,<lid>,<channelNo>,<addr>[,S<station-id>][,C<cray-id>]
    //
    let items = value.split(",");
    if (items.length >= 4) {
      newCrsInfo.lid       = items[1];
      newCrsInfo.channel   = parseInt(items[2], 8);
      newCrsInfo.stationId = "FE";
      newCrsInfo.crayId    = "C1";
      for (let i = 4; i < items.length; i++) {
        if (items[i].startsWith("C")) {
          newCrsInfo.crayId = items[i].substring(1);
        }
        else if (items[i].startsWith("S")) {
          newCrsInfo.stationId = items[i].substring(1);
        }
      }
    }
  }
  return Promise.resolve();
};

/*
 * processPasswordChanges
 *
 * Process properties defined in PASSWORDS sections of property files. Detect passwords
 * that have been updated since the last reconfiguration, and synchronize the associated
 * password definitions and references in the running system.
 *
 * Returns:
 *  A promise that is resolved when all password changes have been processed.
 */
const processPasswordChanges = () => {
  if (typeof customProps["PASSWORDS"] !== "undefined") {
    for (const pwDefn of customProps["PASSWORDS"]) {
      let ei = pwDefn.indexOf("=");
      if (ei > 0) {
        let un = pwDefn.substring(0, ei).toUpperCase().trim();
        let pw = pwDefn.substring(ei + 1).toUpperCase().trim();
        if (typeof passwordMap[un] === "undefined" || passwordMap[un] !== pw) {
          updatedPasswords.push(un);
          passwordMap[un] = pw;
        }
      }
    }
    if (updatedPasswords.length > 0) {
      const products = JSON.parse(fs.readFileSync("opt/products.json", "utf8"));
      let installedProducts = [];
      if (fs.existsSync("opt/installed.json")) {
        installedProducts = JSON.parse(fs.readFileSync("opt/installed.json", "utf8"));
      }
      let processors = {};
      for (const category of products) {
        for (const prodDefn of category.products) {
          if (typeof prodDefn.users !== "undefined" && utilities.isInstalled(prodDefn.name)) {
            for (const un of Object.keys(prodDefn.users)) {
              if (updatedPasswords.indexOf(un) >= 0) {
                if (typeof processors[un] === "undefined") processors[un] = [];
                processors[un] = processors[un].concat(prodDefn.users[un]);
              }
            }
          }
        }
      }
      let promise = dtc.say("Update passwords ...");
      for (const un of updatedPasswords) {
        promise = promise
        .then(() => dtc.say(`  ${un}`))
        .then(() => dtc.dsd(`X.MODVAL(OP=Z)/${un},PW=${passwordMap[un]}`));
      }
      for (const un of Object.keys(processors).sort()) {
        for (const item of processors[un]) {
          if (item.endsWith(".job")) {
            promise = promise
              .then(() => dtc.say(`Run job opt/${item} ...`))
              .then(() => dtc.runJob(12, 4, `opt/${item}`));
          }
          else {
            promise = promise
            .then(() => dtc.say(`Run command "node opt/${item}" ...`))
            .then(() => dtc.disconnect())
            .then(() => dtc.exec("node", [`opt/${item}`]))
            .then(() => dtc.connect())
            .then(() => dtc.expect([ {re:/Operator> $/} ]));
          }
        }
      }
      return promise
      .then(() => {
        fs.writeFileSync("opt/password-map.json", JSON.stringify(passwordMap));
        return Promise.resolve();
      });
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
  if (typeof options.username === "undefined" && typeof options.user === "undefined") {
    options.username = "INSTALL";
    options.password = utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL");
  }
  return dtc.createJobWithOutput(12, 4, job, options);
};

/*
 * updateLIDCMxx
 *
 * Create/Update the LIDCMxx file to reflect the machine's new identifier, if any.
 *
 * Returns:
 *  A promise that is resolved when the LIDCMxx file has been updated.
 */
const updateLIDCMxx = () => {
  if (oldMID !== newMID && newMID !== null) {
    return dtc.say(`Create LIDCM${newMID} ...`)
    .then(() => utilities.getFile(dtc, `LIDCM${oldMID}/UN=SYSTEMX`))
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
    .then(() => utilities.moveFile(dtc, `LIDCM${newMID}`, 1, 377777));
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
      username: "INSTALL",
      password: utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL"),
      data:    `${productRecords.join("~eor\n")}`
    };
    return dtc.say("Update PRODUCT ...")
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

  if ((oldMID === newMID || newMID === null)
      && (oldHostID === newHostID || newHostID === null)
      && typeof customProps["HOSTS"] === "undefined") {
    return Promise.resolve();
  }
  else {
    return dtc.say("Update TCPHOST ...")
    .then(() => utilities.getFile(dtc, "TCPHOST", {username:"NETADMN",password:utilities.getPropertyValue(customProps, "PASSWORDS", "NETADMN", "NETADMN")}))
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
      if (typeof customProps["HOSTS"] !== "undefined") {
        for (const defn of customProps["HOSTS"]) {
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
      return text;
    })
    .then(text => {
      const job = [
        "$CHANGE,TCPHOST/CT=PU,M=R,AC=Y."
      ];
      const netadmnPw = utilities.getPropertyValue(customProps, "PASSWORDS", "NETADMN", "NETADMN");
      const options = {
        jobname:  "MAKEPUB",
        username: "NETADMN",
        password: netadmnPw
      };
      return dtc.putFile("TCPHOST/IA", text, {username:"NETADMN",password:netadmnPw})
      .then(() => dtc.createJobWithOutput(12, 4, job, options));
    });
  }
};

/*
 * updateTcpResolver
 *
 * If a RESOLVER property is defined, create/update the TCPRSLV file to reflect the TCP/IP
 * resource resolver defined by it.
 *
 * Returns:
 *  A promise that is resolved when the TCPRSLV file has been updated.
 */
const updateTcpResolver = () => {
  if (typeof customProps["RESOLVER"] !== "undefined") {
    const job = [
      "$CHANGE,TCPRSLV/CT=PU,M=R,AC=Y."
    ];
    const netadmnPw = utilities.getPropertyValue(customProps, "PASSWORDS", "NETADMN", "NETADMN");
    const options = {
      jobname: "MAKEPUB",
      username: "NETADMN",
      password: netadmnPw
    };
    return dtc.say("Create/Update TCPRSLV ...")
    .then(() => dtc.putFile("TCPRSLV/IA", `${customProps["RESOLVER"].join("\n")}\n`, {username:"NETADMN",password:netadmnPw}))
    .then(() => dtc.createJobWithOutput(12, 4, job, options));
  }
  else {
    return Promise.resolve();
  }
};

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => processPasswordChanges())
.then(() => processCmrdProps())
.then(() => processEqpdProps())
.then(() => processNetworkProps())
.then(() => updateProductRecords())
.then(() => updateLIDCMxx())
.then(() => updateTcpResolver())
.then(() => dtc.disconnect())
.then(() => dtc.exec("node", ["opt/rhp-update-ndl"]))
.then(() => {
  return utilities.isInstalled("cybis-base") ? dtc.exec("node", ["opt/cybis-update-ndl"]) : Promise.resolve();
})
.then(() => {
  return utilities.isInstalled("njf") ? dtc.exec("node", ["opt/njf-update-ndl"]) : Promise.resolve();
})
.then(() => {
  return utilities.isInstalled("rbf5") ? dtc.exec("node", ["opt/rbf5-update-ndl"]) : Promise.resolve();
})
.then(() => {
  return utilities.isInstalled("tlf") ? dtc.exec("node", ["opt/tlf-update-ndl"]) : Promise.resolve();
})
.then(() => dtc.exec("node", ["compile-ndl"]))
.then(() => dtc.exec("node", ["rhp-configure", "-ndl"]))
.then(() => {
  return utilities.isInstalled("njf") ? dtc.exec("node", ["njf-configure", "-ndl"]) : Promise.resolve();
})
.then(() => {
  return utilities.isInstalled("rbf5") ? dtc.exec("node", ["rbf-configure", "-ndl"]) : Promise.resolve();
})
.then(() => {
  return utilities.isInstalled("tlf") ? dtc.exec("node", ["tlf-configure", "-ndl"]) : Promise.resolve();
})
.then(() => {
  return utilities.isInstalled("mailer") ? dtc.exec("node", ["mailer-configure"]) : Promise.resolve();
})
.then(() => {
  if (utilities.isInstalled("netmail")) {
    return dtc.exec("node", ["netmail-configure"])
    .then(() => {
      return (oldHostID !== newHostID && newHostID !== null)
        ? dtc.connect()
          .then(() => dtc.expect([ {re:/Operator> $/} ]))
          .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
          .then(() => dtc.say("Update e-mail address registrations ..."))
          .then(() => dtc.runJob(12, 4, "opt/netmail-reregister-addresses.job", [oldHostID, newHostID]))
          .then(() => dtc.disconnect())
        : Promise.resolve();
    });
  }
  else {
    return Promise.resolve();
  }
})
.then(() => {
  if (utilities.isInstalled("crs") && typeof newCrsInfo.lid !== "undefined") {
    if (   oldCrsInfo.lid       !== newCrsInfo.lid
        || oldCrsInfo.stationId !== newCrsInfo.stationId
        || oldCrsInfo.crayId    !== newCrsInfo.crayId
        || updatedPasswords.indexOf("BCSCRAY") >= 0) {
      return dtc.say("Rebuild CRS ...")
      .then(() => dtc.exec("node", ["install-product","-f","crs"]));
    }
    else if (oldCrsInfo.channel !== newCrsInfo.channel) {
      return dtc.say("Update CRS ...")
      .then(() => dtc.exec("node", ["opt/crs.post"]));
    }
  }
  return Promise.resolve();
})
.then(() => {
  utilities.purgeCache();
  if (fs.existsSync("cyber.ovl")) {
    dtc.readPropertyFile("cyber.ovl", ovlProps);
  }
  return Promise.resolve();
})
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => updateTcpHosts())
.then(() => utilities.getHostRecord(dtc))
.then(hostRecord => {
  const tokens = hostRecord.split(/\s+/);
  newIpAddress = tokens[0];
  let oldIpAddress = utilities.getPropertyValue(iniProps, "cyber", "ipAddress", "127.0.0.1");
  if (newIpAddress !== oldIpAddress) {
    //
    // Create/update [cyber] section to define ipAddress
    //
    let ovlText = [];
    if (typeof ovlProps["cyber"] !== "undefined") {
      for (const line of ovlProps["cyber"]) {
        if (!line.startsWith("ipAddress=")) {
          ovlText.push(line);
        }
      }
    }
    ovlText.push(`ipAddress=${newIpAddress}`);
    ovlProps["cyber"] = ovlText;

    //
    // Create/update [manual] section to define ipAddress
    //
    ovlText = [];
    if (typeof ovlProps["manual"] !== "undefined") {
      for (const line of ovlProps["manual"]) {
        if (!line.startsWith("ipAddress=")) {
          ovlText.push(line);
        }
      }
    }
    ovlText.push(`ipAddress=${newIpAddress}`);
    ovlProps["manual"] = ovlText;
  }
  return Promise.resolve();
})
.then(() => {
  //
  // If the [NETWORK] section of site.cfg includes a "networkInterface" definition
  // identifying a TAP interface, update cyber.ovl to include a corresponding
  // definition.
  //
  let ifc = utilities.getPropertyValue(customProps, "NETWORK", "networkInterface", null);
  if (ifc !== null) {
    let tapmgrPath = tapMgrs[os.type()];
    let ci = ifc.indexOf(",");
    if (ci !== -1) {
      tapmgrPath = ifc.substring(ci + 1).trim();
      ifc = ifc.substring(0, ci).trim();
    }
    if ( /^tap[0-9]+$/.test(ifc) && typeof tapmgrPath !== "undefined") {
      let ovlText = [];
      if (typeof ovlProps["cyber"] !== "undefined") {
        for (const line of ovlProps["cyber"]) {
          if (!line.startsWith("networkInterface=")) {
            ovlText.push(line);
          }
        }
      }
      ovlText.push(`networkInterface=${ifc},${tapmgrPath}`);
      ovlProps["cyber"] = ovlText;
      ovlText = [];
      if (typeof ovlProps["manual"] !== "undefined") {
        for (const line of ovlProps["manual"]) {
          if (!line.startsWith("networkInterface=")) {
            ovlText.push(line);
          }
        }
      }
      ovlText.push(`networkInterface=${ifc},${tapmgrPath}`);
      ovlProps["manual"] = ovlText;
    }
  }
  return Promise.resolve();
})
.then(() => utilities.getHosts(dtc))
.then(hosts => {
  //
  // Update Automated Cartridge Tape device definitions in cyber.ovl and
  // equipment deck if the tape server's address or the drive path has
  // changed.
  //
  let oldStkHost     = "127.0.0.1";
  let oldStkPort     = 4400;
  let oldStkModule   = 0;
  let oldStkPanel    = 0;
  let oldStkDrive    = 0
  if (typeof iniProps["equipment.nos287"] !== "undefined") {
    for (const line of iniProps["equipment.nos287"]) {
      if (line.startsWith("MT5744,")) {
        let tokens    = line.split(",");
        let ci        = tokens[4].indexOf(":");
        let si        = tokens[4].indexOf("/");
        oldStkHost    = tokens[4].substring(0, ci);
        oldStkPort    = parseInt(tokens[4].substring(ci + 1, si));
        let drivePath = tokens[4].substring(si + 1);
        let match     = drivePath.match(/^M([0-7]+)P([0-7]+)D([0-7]+)$/);
        if (match !== null) {
          oldStkModule = parseInt(match[1], 8);
          oldStkPanel  = parseInt(match[2], 8);
          oldStkDrive  = parseInt(match[3], 8);
          break;
        }
      }
    }
  }
  let newStkHost = oldStkHost;
  let newStkPort = oldStkPort;
  for (const ipAddress of Object.keys(hosts)) {
    for (const name of hosts[ipAddress]) {
      if (name.toUpperCase() === "STK") {
        newStkHost = ipAddress;
        if (newStkHost !== oldStkHost) newStkPort = 4400;
        break;
      }
    }
  }
  let newStkModule = oldStkModule;
  let newStkPanel  = oldStkPanel;
  let newStkDrive  = oldStkDrive;
  let drivePath = utilities.getPropertyValue(customProps, "NETWORK", "stkDrivePath", null);
  if (drivePath !== null) {
    let si = drivePath.indexOf("/");
    if (si >= 0) {
      newStkPort = parseInt(drivePath.substring(0, si));
      drivePath = drivePath.substring(si + 1);
    }
    let match = drivePath.match(/^M([0-7]+)P([0-7]+)D([0-7]+)$/i);
    if (match !== null) {
      newStkModule = parseInt(match[1], 8);
      newStkPanel  = parseInt(match[2], 8);
      newStkDrive  = parseInt(match[3], 8);
    }
  }
  const isPathChange = newStkModule !== oldStkModule
    || newStkPanel !== oldStkPanel
    || newStkDrive !== oldStkDrive;
  if (newStkHost !== oldStkHost || newStkPort !== oldStkPort || isPathChange) {
    //
    // Create/update [equipment.nos287] section to define MT5744 entries
    //
    let ovlText = [];
    if (typeof ovlProps["equipment.nos287"] !== "undefined") {
      for (const line of ovlProps["equipment.nos287"]) {
        if (!line.startsWith("MT5744,")) {
          ovlText.push(line);
        }
      }
    }
    for (let i = 0; i < 4; i++) {
      ovlText.push(`MT5744,0,${i},23,${newStkHost}:${newStkPort}/M${newStkModule.toString(8)}P${newStkPanel.toString(8)}D${(newStkDrive + i).toString(8)}`);
    }
    ovlProps["equipment.nos287"] = ovlText;
  }
  if (isPathChange) {
    //
    // Update equipment deck
    //
    return dtc.say("Update automated cartridge tape equipment definition in EQPD01 ...")
    .then(() => utilities.getSystemRecord(dtc, "EQPD01"))
    .then(eqpd01 => {
      eqpd01 = eqpd01.replace(/EQ060=AT-4,UN=0,CH=23,LS=[0-7]+,PA=[0-7]+,DR=[0-7]+\./,
        `EQ060=AT-4,UN=0,CH=23,LS=${newStkModule.toString(8)},PA=${newStkPanel.toString(8)},DR=${(newStkDrive).toString(8)}.`);
      const job = [
        "$COPY,INPUT,EQPD01.",
        "$REWIND,EQPD01.",
        "$ATTACH,PRODUCT/M=W,WB.",
        "$LIBEDIT,P=PRODUCT,B=EQPD01,I=0,C."
      ];
      const options = {
        jobname: "UPDEQPD",
        username: "INSTALL",
        password: utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL"),
        data: eqpd01
      };
      return dtc.createJobWithOutput(12, 4, job, options);
    });
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.say("Make a new deadstart tape ..."))
.then(() => dtc.disconnect())
.then(() => dtc.exec("node", ["make-ds-tape"]))
.then(() => dtc.connect())
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Shutdown the system to re-deadstart using the new tape ..."))
.then(() => dtc.shutdown(false))
.then(() => dtc.sleep(5000))
.then(() => dtc.say("Rename tapes/newds.tap to tapes/ds.tap ..."))
.then(() => {
  if (fs.existsSync("tapes/ods.tap")) {
    fs.unlinkSync("tapes/ods.tap");
  }
  fs.renameSync("tapes/ds.tap", "tapes/ods.tap");
  fs.renameSync("tapes/newds.tap", "tapes/ds.tap");
  return Promise.resolve();
})
.then(() => {
  let keys = Object.keys(ovlProps);
  if (keys.length > 0) {
    let lines = [];
    for (const key of Object.keys(ovlProps)) {
      lines.push(`[${key}]`);
      for (const line of ovlProps[key]) {
        lines.push(`${line}`);
      }
    }
    lines.push("");
    fs.writeFileSync("cyber.ovl", lines.join("\n"));
    return dtc.say("Create/update cyber.ovl ...");
  }
  else {
    return Promise.resolve();
  }
})
.then(() => dtc.say("Deadstart using the new tape ..."))
.then(() => {
  if (oldMID !== newMID && newMID !== null) {
    return dtc.start(["manual"], {
      detached: true,
      stdio:    [0, "ignore", 2],
      unref:    false
    })
    .then(() => dtc.sleep(5000))
    .then(() => dtc.connect(newIpAddress))
    .then(() => dtc.expect([{ re: /Operator> $/ }]))
    .then(() => dtc.attachPrinter("LP5xx_C12_E5"))
    .then(() => dtc.dsd([
      "O!",
      "#1000#P!",
      "#1000#D=YES",
      "#1000#",
      "#5000#NEXT.",
      "#1000#]!",
      "#1000#INITIALIZE,AL,6.",
      "#1000#GO.",
      "#7500#%year%%mon%%day%",
      "#3000#%hour%%min%%sec%"
    ]));
  }
  else {
    return dtc.start({
      detached: true,
      stdio:    [0, "ignore", 2],
      unref:    false
    })
    .then(() => dtc.sleep(5000))
    .then(() => dtc.connect(newIpAddress))
    .then(() => dtc.expect([{ re: /Operator> $/ }]))
    .then(() => dtc.attachPrinter("LP5xx_C12_E5"));
  }
})
.then(() => dtc.expect([{ re: /QUEUE FILE UTILITY COMPLETE/ }], "printer"))
.then(() => dtc.say("Deadstart complete"))
.then(() => dtc.say("Reconfiguration complete"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
