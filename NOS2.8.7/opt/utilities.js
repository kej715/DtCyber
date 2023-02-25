const fs = require("fs");

const utilities = {

  clearProgress: maxProgressLen => {
    let progress = `\r`;
    while (progress.length++ < maxProgressLen) progress += " ";
    process.stdout.write(`${progress}\r`);
    return Promise.resolve();
  },

  enableSubsystem: (dtc, name, cp) => {
    const job = [
      "$ATTACH,PRODUCT/WB.",
      "$NOEXIT.",
      "$GTR,PRODUCT,IPRD01.IPRD01",
      "$ONEXIT.",
      "$IF,.NOT.FILE(IPRD01,AS),GTRSYS.",
      "$  COMMON,SYSTEM.",
      "$  GTR,SYSTEM,IPRD01.IPRD01",
      "$ENDIF,GTRSYS.",
      "$REWIND,IPRD01.",
      "$COPYSBF,IPRD01."
    ];
    return dtc.createJobWithOutput(12, 4, job, {jobname:"GETIPRD"})
    .then(iprd01 => {
      let si = 0;
      while (si < iprd01.length) {
        let ni = iprd01.indexOf("\n", si);
        if (ni < 0) ni = iprd01.length;
        let line = iprd01.substring(si, ni).trim();
        if (line === `DISABLE,${name}.`
            || line.startsWith(`DISABLE,${name},`)
            || line.startsWith(`ENABLE,${name},`)) {
          iprd01 = `${iprd01.substring(0, si)}ENABLE,${name},${cp.toString(8)}.\n${iprd01.substring(ni + 1)}`;
          break;
        }
        si = ni + 1;
      }
      const job = [
        "$COPY,INPUT,IPRD01.",
        "$REWIND,IPRD01.",
        "$ATTACH,PRODUCT/M=W,WB.",
        "$LIBEDIT,P=PRODUCT,B=IPRD01,I=0,C."
      ];
      const options = {
        jobname: "UPDIPRD",
        data:    iprd01
      };
      return dtc.createJobWithOutput(12, 4, job, options);
    });
  },

  getCouplerNode: dtc => {
    const hostId      = utilities.getHostId(dtc);
    const rhpTopology = utilities.getRhpTopology(dtc);
    return rhpTopology[hostId].couplerNode;
  },

  getCustomProperties: dtc => {
    if (typeof utilities.customProperties === "undefined") {
      utilities.customProperties = {};
      if (fs.existsSync("site.cfg")) {
        dtc.readPropertyFile("site.cfg", utilities.customProperties);
      }
      else if (fs.existsSync("../site.cfg")) {
        dtc.readPropertyFile("../site.cfg", utilities.customProperties);
      }
    }
    return utilities.customProperties;
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

  getHostId: dtc => {
    if (typeof utilities.hostId !== "undefined") return utilities.hostId;
    const iniProps = utilities.getIniProperties(dtc);
    const pattern = /^HOSTID=([^.]*)/i;
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
    const customProps = utilities.getCustomProperties(dtc);
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
    utilities.hostId = hostId !== null ? hostId : `M${utilities.getMachineId(dtc)}`;
    return utilities.hostId;
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
      const customProps = utilities.getCustomProperties(dtc);
      if (typeof customProps["HOSTS"] !== "undefined") {
        for (const defn of customProps["HOSTS"]) {
          if (/^[0-9]/.test(defn) && pattern.test(defn)) {
            hostRecord = defn;
            break;
          }
        }
      }
      return hostRecord;
    });
  },

  getIniProperties: dtc => {
    if (typeof utilities.iniProperties === "undefined") {
      utilities.iniProperties = {};
      let pathname = "cyber.ini";
      if (!fs.existsSync(pathname)) pathname = "../cyber.ini";
      dtc.readPropertyFile(pathname, utilities.iniProperties);
      if (fs.existsSync("cyber.ovl")) {
        dtc.readPropertyFile("cyber.ovl", utilities.iniProperties);
      }
      else if (fs.existsSync("../cyber.ovl")) {
        dtc.readPropertyFile("../cyber.ovl", utilities.iniProperties);
      }
    }
    return utilities.iniProperties;
  },

  getMachineId: dtc => {
    if (typeof utilities.machineId !== "undefined") return utilities.machineId;
    const customProps = utilities.getCustomProperties(dtc);
    if (typeof customProps["CMRDECK"] !== "undefined") {
      for (let line of customProps["CMRDECK"]) {
        line = line.trim().toUpperCase();
        let match = line.match(/^MID=([^.]*)/);
        if (match !== null) {
          utilities.machineId = match[1].trim();
          return utilities.machineId;
        }
      }
    }
    utilities.machineId = "01";
    return utilities.machineId;
  },

  getNpuNode: dtc => {
    const hostId      = utilities.getHostId(dtc);
    const rhpTopology = utilities.getRhpTopology(dtc);
    return rhpTopology[hostId].npuNode;
  },

  getRhpTopology: dtc => {
    if (typeof utilities.rhpTopology !== "undefined") return utilities.rhpTopology;
    utilities.rhpTopology = {};
    const hostId    = utilities.getHostId(dtc);
    const mid       = utilities.getMachineId(dtc);
    const iniProps  = utilities.getIniProperties(dtc);
    let couplerNode = 1;
    let npuNode     = 2;
    if (typeof iniProps["npu.nos287"] !== "undefined") {
      for (let line of iniProps["npu.nos287"]) {
        line = line.trim().toUpperCase();
        let ei = line.indexOf("=");
        if (ei < 0) continue;
        let key   = line.substring(0, ei).trim();
        let value = line.substring(ei + 1).trim();
        if (key === "COUPLERNODE") {
          couplerNode = parseInt(value);
        }
        else if (key === "NPUNODE") {
          npuNode = parseInt(value);
        }
      }
    }
    let localNode = {
      name: hostId,
      lid: `M${mid}`,
      addr: "127.0.0.1:2550",
      couplerNode: couplerNode,
      npuNode: npuNode,
      links: {}
    };
    utilities.rhpTopology[hostId] = localNode;
    const customProps = utilities.getCustomProperties(dtc);
    if (typeof customProps["NETWORK"] !== "undefined") {
      for (let line of customProps["NETWORK"]) {
        line = line.toUpperCase();
        let ei = line.indexOf("=");
        if (ei < 0) continue;
        let key   = line.substring(0, ei).trim();
        let value = line.substring(ei + 1).trim();
        if (key === "RHPNODE") {
          //
          //  rhpNode=<host-id>,<lid>,<addr>,<coupler-node>,<npu-node>,<remote-host-id>,<cla-port>[,<remote-host-id>,<cla-port>...]
          //
          let items = value.split(",");
          if (items.length >= 7) {
            let node = {
              name: items.shift(),
              lid: items.shift(),
              addr: items.shift(),
              couplerNode: parseInt(items.shift()),
              npuNode: parseInt(items.shift()),
              links: {}
            };
            while (items.length >= 2) {
              let linkName = items.shift();
              node.links[linkName] = parseInt(items.shift());
            };
            if (items.length > 0) {
              throw new Error(`Invalid rhpNode definition: CLA port missing for link ${items.shift()}`);
            }
            utilities.rhpTopology[node.name] = node;
          }
        }
      }
    }
    return utilities.rhpTopology;
  },

  getSystemRecord: (dtc, rid, options) => {
    const job = [
      "$COMMON,SYSTEM.",
      `$GTR,SYSTEM,REC.${rid}`,
      "$REWIND,REC.",
      "$COPYSBF,REC."
    ];
    if (typeof options === "undefined") options = {};
    options.jobname = "GTRSYS";
    return dtc.createJobWithOutput(12, 4, job, options);
  },

  getTimeZone: () => {
    if (typeof utilities.timeZone !== "undefined") return utilities.timeZone;
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
          utilities.timeZone = dstTimeZones[offset];
        }
      }
      else if (typeof stdTimeZones[offset] !== "undefined") {
        utilities.timeZone = stdTimeZones[offset];
      }
    }
    return utilities.timeZone;
  },

  isInstalled: product => {
    if (fs.existsSync("opt/installed.json")) {
      const installedProductSet = JSON.parse(fs.readFileSync("opt/installed.json", "utf8"));
      return installedProductSet.indexOf(product) >= 0;
    }
    return false;
  },

  moveFile: (dtc, fileName, fromUi, toUi, opts) => {
    if (typeof opts === "undefined") opts = "/CT=PU,AC=Y";
    let cmds = [
      "GET,MOVPROC.",
      "PURGE,MOVPROC."
    ];
    if (fromUi !== 1) cmds.push(`SUI,${fromUi}.`);
    cmds.push(`MOVPROC,${fileName},${toUi}.`);
    return dtc.say(`Move ${fileName} from UI ${fromUi} to UI ${toUi}`)
    .then(() => dtc.runJob(12, 4, "opt/move-proc.job", opts))
    .then(() => dtc.dis(cmds, 1))
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
