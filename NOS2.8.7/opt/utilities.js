const fs = require("fs");

const utilities = {

  clearProgress: maxProgressLen => {
    let progress = `\r`;
    while (progress.length++ < maxProgressLen) progress += " ";
    process.stdout.write(`${progress}\r`);
    return Promise.resolve();
  },

  getHostDomainName: dtc => {
    return utilities.getHostRecord(dtc)
    .then(record => {
      const tokens = record.split(/\s+/).slice(1);
      domainName = tokens[0];
      if (domainName.indexOf(".") < 0) {
        for (const token of tokens) {
          if (token.indexOf(".") >= 0) {
            domainName = token;
            break;
          }
        }
      }
      return domainName;
    });
  },

  getFile: (dtc, filename, options) => {
    const job = [
      `$GET,FILE=${filename}.`,
      "$COPYSBF,FILE."
    ];
    if (typeof options === "undefined") options = {};
    options.jobname = "GETFILE";
    return dtc.createJobWithOutput(12, 4, job, options);
  },

  getHostRecord: dtc => {
    return utilities.getFile(dtc, "TCPHOST/UN=NETADMN")
    .then(text => {
      //
      // Find host record in TCPHOST or [HOSTS] section of custom props
      //
      text           = dtc.cdcToAscii(text);
      const mid      = utilities.getMachineId(dtc);
      let hostRecord = `127.0.0.1 M${mid} LOCALHOST_${mid}`;
      const pattern  = new RegExp(`LOCALHOST_${mid}`, "i");
      for (let line of text.split("\n")) {
        line = line.trim();
        if (/^[0-9]/.test(line) && pattern.test(line)) {
          hostRecord = line;
          break;
        }
      }
      let customProps = {};
      dtc.readPropertyFile(customProps);
      if (typeof customProps["HOSTS"] !== "undefined") {
        for (const defn of customProps["HOSTS"].split("\n")) {
          if (/^[0-9]/.test(defn) && pattern.test(defn)) {
            hostRecord = defn;
            break;
          }
        }
      }
      return hostRecord;
    });
  },

  getHostId: dtc => {
    let iniProps = {};
    const pattern = /^HOSTID=([^.]*)/i;
    dtc.readPropertyFile("cyber.ini", iniProps);
    if (fs.existsSync("cyber.ovl")) {
      dtc.readPropertyFile("cyber.ovl", iniProps);
    }
    let hostId = null;
    if (typeof iniProps["npu.nos287"] !== "undefined") {
      for (let line of iniProps["npu.nos287"]) {
        line = line.trim();
        let match = line.match(pattern);
        if (match !== null) {
          hostId = match[1].trim();
        }
      }
    }
    let customProps = {};
    dtc.readPropertyFile(customProps);
    if (typeof customProps["NETWORK"] !== "undefined") {
      for (let line of customProps["NETWORK"]) {
        line = line.trim();
        let match = line.match(pattern);
        if (match !== null) {
          hostId = match[1].trim();
          break;
        }
      }
    }
    return hostId !== null ? hostId : `M${utilities.getMachineId(dtc)}`;
  },

  getMachineId: dtc => {
    let customProps = {};
    dtc.readPropertyFile(customProps);
    if (typeof customProps["CMRDECK"] !== "undefined") {
      for (let line of customProps["CMRDECK"]) {
        line = line.trim().toUpperCase();
        let match = line.match(/^MID=([^.]*)/);
        if (match !== null) {
          return match[1].trim();
        }
      }
    }
    return "01";
  },

  getTimeZone: () => {
    const stdTimeZones = {
      "-0800":"PST",
      "-0700":"MST",
      "-0600":"CST",
      "-0500":"EST",
      "-0400":"AST",
      "-0330":"NST",
      "+0000":"GMT",
      "+0100":"CET",
      "+0200":"EET"
    };
    const dstTimeZones = {
      "-0700":"PDT",
      "-0600":"MDT",
      "-0500":"CDT",
      "-0400":"EDT",
      "-0300":"ADT",
      "-0230":"NDT",
      "+0000":"GMT",
      "+0100":"BST",
      "+0200":"CET",
      "+0300":"EET"
    };
    const date    = new Date();
    const jul     = new Date(date.getFullYear(), 6, 1).getTimezoneOffset();
    let  match    = date.toString().match(/GMT([-+][0-9]{4})/);
    let  timeZone = "GMT";
    if (match !== null) {
      let offset = match[1];
      if (jul === date.getTimezoneOffset()) {
        if (typeof dstTimeZones[offset] !== "undefined") {
          timeZone = dstTimeZones[offset];
        }
      }
      else if (typeof stdTimeZones[offset] !== "undefined") {
        timeZone = stdTimeZones[offset];
      }
    }
    return timeZone;
  },

  isInstalled: product => {
    if (fs.existsSync("opt/installed.json")) {
      const installedProductSet = JSON.parse(fs.readFileSync("opt/installed.json", "utf8"));
      return installedProductSet.indexOf(product) >= 0;
    }
    return false;
  },

  moveFile: (dtc, fileName, fromUi, toUi) => {
    return dtc.say(`Move ${fileName} from UI ${fromUi} to UI ${toUi}`)
    .then(() => dtc.runJob(12, 4, "opt/move-proc.job"))
    .then(() => dtc.dis([
      "GET,MOVPROC.",
      "PURGE,MOVPROC.",
      `MOVPROC,${fileName},${toUi}.`
    ], fromUi))
    .then(() => dtc.waitJob("MOVE"));
  },

  reportProgress: (byteCount, contentLength, maxProgressLen) => {
    let progress = `\r${new Date().toLocaleTimeString()}   Received ${byteCount}`;
    if (contentLength === -1) {
      progress += " bytes";
    }
    else {
      progress += ` of ${contentLength} bytes (${Math.round((byteCount / contentLength) * 100)}%)`;
    }
    process.stdout.write(progress)
    return (progress.length > maxProgressLen) ? progress.length : maxProgressLen;
  }
};

module.exports = utilities;
