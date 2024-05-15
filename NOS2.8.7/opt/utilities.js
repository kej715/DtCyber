const fs = require("fs");

const utilities = {

  calculateNjeRoute: (dtc, node) => {
    if (typeof node.route === "undefined") {
      for (const adjacentNode of Object.values(utilities.getAdjacentNjeNodes(dtc))) {
        if (adjacentNode.id === node.link) {
          node.route = adjacentNode.id;
          return node.route;
        }
      }
      if (typeof node.link !== "undefined" && node.link !== utilities.getHostId(dtc)) {
        const route = utilities.calculateNjeRoute(dtc, utilities.njeTopology[node.link]);
        if (route !== null) node.route = route;
        return route;
      }
      else {
        return null;
      }
    }
    else {
      return node.route;
    }
  },

  clearProgress: maxProgressLen => {
    let progress = `\r`;
    while (progress.length++ < maxProgressLen) progress += " ";
    process.stdout.write(`${progress}\r`);
    return Promise.resolve();
  },

  compileNDL: dtc => {
    const mid = utilities.getMachineId(dtc);
    return dtc.say("Compile NDL ...")
    .then(() => dtc.runJob(12, 4, "decks/compile-ndlopl.job", [mid]));
  },

  deleteNDLmod: (dtc, modname) => {
    const job = [
      "$GET,NDLMODS/NA.",
      "$IF,FILE(NDLMODS,AS),EDIT.",
      "$  NOEXIT.",
      `$  LIBEDIT,P=NDLMODS,N=0,C,Z.+*DELETE ${modname}`,
      "$  ONEXIT.",
      "$  REPLACE,NDLMODS.",
      "$ENDIF,EDIT."
    ];
    const options = {
      jobname:  "DELMOD",
      username: "NETADMN",
      password: utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "NETADMN", "NETADMN")
    };
    return dtc.say("Update NDL ...")
    .then(() => dtc.createJobWithOutput(12, 4, job, options));
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
    const options = {
      jobname: "GETIPRD",
      username: "INSTALL",
      password: utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "INSTALL", "INSTALL")
    };
    return dtc.createJobWithOutput(12, 4, job, options)
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
        username: "INSTALL",
        password: utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "INSTALL", "INSTALL"),
        data:    iprd01
      };
      return dtc.createJobWithOutput(12, 4, job, options);
    });
  },

  getAdjacentNjeNodes: dtc => {
    if (typeof utilities.adjacentNjeNodes === "undefined") {
      const topology = utilities.getNjeTopology(dtc);
    }
    return utilities.adjacentNjeNodes;
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

  getDefaultNjeRoute: dtc => {
    if (typeof utilities.defaultNjeRoute === "undefined") {
      utilities.defaultNjeRoute = utilities.getPropertyValue(utilities.getCustomProperties(dtc), "NETWORK", "defaultRoute", null);
      if (utilities.defaultNjeRoute === null) {
        const hostID = utilities.getHostId(dtc);
        let localNode = utilities.njeTopology[hostID];
        if (typeof localNode.link !== "undefined") {
          utilities.defaultNjeRoute = localNode.link;
        }
        else {
          utilities.defaultNjeRoute = hostID;
        }
      }
      utilities.defaultNjeRoute = utilities.defaultNjeRoute.toUpperCase();
    }
    return utilities.defaultNjeRoute;
  },

  getFile: (dtc, filename, options) => {
    const job = [
      `$GET,FILE=${filename}.`,
      "$COPYSBF,FILE."
    ];
    if (typeof options === "undefined") options = {};
    options.jobname = "GETFILE";
    if (typeof options.username === "undefined" && typeof options.user === "undefined") {
      options.username = "INSTALL";
      options.password = utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "INSTALL", "INSTALL");
    }
    return dtc.createJobWithOutput(12, 4, job, options);
  },

  getHaspTerminals: dtc => {

    if (typeof utilities.haspTerminals !== "undefined") return utilities.haspTerminals;

    const customProps = utilities.getCustomProperties(dtc);
    let   nextPort    = 0x26;
    let   portCount   = 2;
    let   terminals   = [];

    if (typeof customProps["NETWORK"] !== "undefined") {
      for (let line of customProps["NETWORK"]) {
        line = line.toUpperCase();
        let ei = line.indexOf("=");
        if (ei < 0) continue;
        let key   = line.substring(0, ei).trim();
        let value = line.substring(ei + 1).trim();
        if (key === "HASPTERMINAL") {
          //
          //  haspTerminal=<name>,<tcp-port>,[,B<block-size>]
          //
          let items = value.split(",");
          if (items.length >= 2) {
            let terminal = {
              id:        items.shift(),
              tcpPort:   parseInt(items.shift()),
              blockSize: 400
            };
            while (items.length > 0) {
              let item = items.shift();
              if (item.startsWith("B")) {
                terminal.blockSize = parseInt(item.substring(1));
              }
            }
            terminals.push(terminal);
          }
        }
        else if (key === "HASPPORTS") {
          let items = value.split(",");
          if (items.length > 0) nextPort  = parseInt(items.shift());
          if (items.length > 0) portCount = parseInt(items.shift());
        }
      }
    }
    terminals.sort((t1, t2) => {
      let p1 = t1.tcpPort;
      let p2 = t2.tcpPort;
      if (p1 < p2) {
        return -1;
      }
      else if (p1 > p2) {
        return 1;
      }
      else if (t1.id.toUpperCase() < t2.id.toUpperCase()) {
        return -1;
      }
      else if (t1.id.toUpperCase() > t2.id.toUpperCase()) {
        return 1;
      }
      else {
        return 0;
      }
    });
    for (const terminal of terminals) {
      if (portCount < 1) throw new Error("Insufficient number of HASP ports defined");
      terminal.claPort = nextPort++;
      portCount -= 1;
    }
    utilities.haspTerminals = terminals;
    return utilities.haspTerminals;
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
    const iniProps = dtc.getIniProperties(dtc);
    let hostId = utilities.getPropertyValue(iniProps, "npu.nos287", "hostID", null);
    hostId = utilities.getPropertyValue(utilities.getCustomProperties(dtc), "NETWORK", "hostID", hostId);
    utilities.hostId = hostId !== null ? hostId.toUpperCase() : `M${utilities.getMachineId(dtc)}`;
    return utilities.hostId;
  },

  getHostRecord: dtc => {
    return utilities.getHosts(dtc)
    .then(hosts => {
      //
      // Find host record among TCPHOST and [HOSTS] section of custom props
      //
      const mid       = utilities.getMachineId(dtc);
      const localhost = `LOCALHOST_${mid}`;
      for (const ipAddress of Object.keys(hosts)) {
        let names = hosts[ipAddress];
        for (const name of names) {
          if (name.toUpperCase() === localhost) {
            return `${ipAddress} ${names.join(" ")}`;
          }
        }
      }
      return `${dtc.getHostIpAddress()} M${mid} ${localhost}`;
    });
  },

  getHosts: dtc => {
    if (typeof utilities.hosts !== "undefined") return utilities.hosts;
    utilities.hosts = {};
    return utilities.getFile(dtc, "TCPHOST/UN=NETADMN")
    .then(text => {
      text = dtc.cdcToAscii(text);
      for (let line of text.split("\n")) {
        line = line.trim();
        if (/^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/.test(line)) {
          let tokens = line.split(/\s+/);
          utilities.hosts[tokens[0]] = tokens.slice(1);
        }
      }
      const customProps = utilities.getCustomProperties(dtc);
      if (typeof customProps["HOSTS"] !== "undefined") {
        for (const defn of customProps["HOSTS"]) {
          if (/^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/.test(defn)) {
            let tokens = defn.split(/\s+/);
            utilities.hosts[tokens[0]] = tokens.slice(1);
          }
        }
      }
      return utilities.hosts;
    });
  },

  getMachineId: dtc => {
    if (typeof utilities.machineId !== "undefined") return utilities.machineId;
    let mid = utilities.getPropertyValue(utilities.getCustomProperties(dtc), "CMRDECK", "MID", "01");
    utilities.machineId = mid.substring(0, 2).toUpperCase();
    return utilities.machineId;
  },

  getNjeTopology: dtc => {
    if (typeof utilities.njeTopology === "undefined") {
      if (fs.existsSync("files/nje-topology.json")) {
        utilities.njeTopology = JSON.parse(fs.readFileSync("files/nje-topology.json"));
      }
      else if (fs.existsSync("../files/nje-topology.json")) {
        utilities.njeTopology = JSON.parse(fs.readFileSync("../files/nje-topology.json"));
      }
      else {
        utilities.njeTopology = {};
      }
      for (const name of Object.keys(utilities.njeTopology)) {
        let node  = utilities.njeTopology[name];
        node.id   = name;
        node.type = "NJE";
      }
      utilities.adjacentNjeNodes               = {};
      utilities.nonadjacentNjeNodesWithLids    = {};
      utilities.nonadjacentNjeNodesWithoutLids = {};
 
      const hostID    = utilities.getHostId(dtc);
      const mid       = utilities.getMachineId(dtc);
      let   topology  = utilities.njeTopology;
      let   nextPort  = 0x30;
      let   portCount = 16;

      const customProps = utilities.getCustomProperties(dtc);
      if (typeof customProps["NETWORK"] !== "undefined") {
        for (let line of customProps["NETWORK"]) {
          line = line.toUpperCase();
          let ei = line.indexOf("=");
          if (ei < 0) continue;
          let key   = line.substring(0, ei).trim();
          let value = line.substring(ei + 1).trim();
          if (key === "DEFAULTROUTE") {
            utilities.defaultNjeRoute = value;
          }
          else if (key === "NJEPORTS") {
            let items = value.split(",");
            if (items.length > 0) nextPort  = parseInt(items.shift());
            if (items.length > 0) portCount = parseInt(items.shift());
          }
          else if (key === "NJENODE") {
            //
            //  njeNode=<nodename>,<software>,<lid>,<public-addr>,<link>
            //     [,<local-address>][,B<block-size>][,P<ping-interval>][,<mailer-address>]
            let items = value.split(",");
            if (items.length >= 5) {
              let nodeName = items.shift();
              let node = {
                id: nodeName,
                type: "NJE",
                software: items.shift(),
                lid: items.shift(),
                publicAddress: items.shift(),
                link: items.shift()
              };
              if (items.length > 0) {
                node.localAddress = items.shift();
              }
              while (items.length > 0) {
                let item = items.shift();
                if (item.startsWith("B")) {
                  node.blockSize = parseInt(item.substring(1));
                }
                else if (item.startsWith("P")) {
                  node.pingInterval = parseInt(item.substring(1));
                }
                else if (item.indexOf("@") > 0) {
                  node.mailer = item;
                }
              }
              topology[nodeName] = node;
            }
          }
        }
      }
      if (typeof topology[hostID] === "undefined") {
        topology[hostID] = {
          id: hostID,
          type: "NJE",
          software: "NJEF",
          lid: `M${mid}`
        };
        if (utilities.defaultNjeRoute !== null) topology[hostID].link = utilities.defaultNjeRoute;
      }
      if (typeof topology[hostID].link !== "undefined") {
        let routingNode = topology[topology[hostID].link];
        utilities.adjacentNjeNodes[topology[hostID].link] = routingNode;
      }
      for (const key of Object.keys(topology).sort()) {
        let node = topology[key];
        if (node.link === hostID) {
          utilities.adjacentNjeNodes[key] = node;
        }
      }
      for (const key of Object.keys(topology)) {
        let node = topology[key];
        if (key !== hostID && typeof utilities.adjacentNjeNodes[key] === "undefined") {
          const route = utilities.calculateNjeRoute(dtc, node);
          if (route === null) node.route = utilities.getDefaultNjeRoute(dtc);
          if (typeof node.lid !== "undefined" && node.lid !== "" && node.lid !== null) {
            utilities.nonadjacentNjeNodesWithLids[key] = node;
          }
          else {
            utilities.nonadjacentNjeNodesWithoutLids[key] = node;
          }
        }
      }
      const toHex = value => {
        return value < 16 ? `0${value.toString(16)}` : value.toString(16);
      };
      let   lineNumber = 1;
      const npuNode    = utilities.getNpuNode(dtc);
      for (const key of Object.keys(utilities.adjacentNjeNodes).sort()) {
        if (portCount < 1) throw new Error("Insufficient number of NJE ports defined");
        let node          = topology[key];
        let portNum       = toHex(nextPort++);
        node.terminalName = `TE${toHex(npuNode)}P${portNum}`;
        node.claPort      = portNum;
        node.lineNumber   = lineNumber++;
        portCount -= 1;
      }
    }
    return utilities.njeTopology;
  },

  getNonadjacentNjeNodesWithLids: dtc => {
    if (typeof utilities.nonadjacentNjeNodesWithLids === "undefined") {
      const topology = utilities.getNjeTopology(dtc);
    }
    return utilities.nonadjacentNjeNodesWithLids;
  },

  getNonadjacentNjeNodesWithoutLids: dtc => {
    if (typeof utilities.nonadjacentNjeNodesWithoutLids === "undefined") {
      const topology = utilities.getNjeTopology(dtc);
    }
    return utilities.nonadjacentNjeNodesWithoutLids;
  },

  getNpuNode: dtc => {
    const hostId      = utilities.getHostId(dtc);
    const rhpTopology = utilities.getRhpTopology(dtc);
    return rhpTopology[hostId].npuNode;
  },

  getPropertyValue: (propertyObject, sectionName, propertyName, defaultValue) => {
    propertyName = propertyName.toUpperCase();
    let value = defaultValue;
    if (typeof propertyObject[sectionName] !== "undefined") {
      for (const line of propertyObject[sectionName]) {
        let ei = line.indexOf("=");
        if (ei === -1) continue;
        let key = line.substring(0, ei).trim().toUpperCase();
        if (key === propertyName) {
          value = line.substring(ei + 1).trim();
          break;
        }
      }
    }
    return value;
  },

  getRhpTopology: dtc => {
    if (typeof utilities.rhpTopology !== "undefined") return utilities.rhpTopology;
    utilities.rhpTopology = {};
    const hostId    = utilities.getHostId(dtc);
    const mid       = utilities.getMachineId(dtc);
    const iniProps  = dtc.getIniProperties(dtc);
    let couplerNode = parseInt(utilities.getPropertyValue(iniProps, "npu.nos287", "couplerNode", "1"));
    let npuNode     = parseInt(utilities.getPropertyValue(iniProps, "npu.nos287", "npuNode",     "2"));
    let localNode = {
      id: hostId,
      type: "RHP",
      software: "NOS",
      lid: `M${mid}`,
      addr: `${dtc.getHostIpAddress()}:2550`,
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
              id: items.shift(),
              lid: items.shift(),
              addr: items.shift(),
              couplerNode: parseInt(items.shift()),
              npuNode: parseInt(items.shift()),
              type: "RHP",
              software: "NOS",
              links: {}
            };
            while (items.length >= 2) {
              let linkName = items.shift();
              node.links[linkName] = parseInt(items.shift());
            };
            if (items.length > 0) {
              throw new Error(`Invalid rhpNode definition: CLA port missing for link ${items.shift()}`);
            }
            utilities.rhpTopology[node.id] = node;
          }
        }
      }
    }
    return utilities.rhpTopology;
  },

  getSystemRecord: (dtc, rid, options) => {
    const job = [
      "$ATTACH,PRODUCT/WB.",
      "$NOEXIT.",
      `$GTR,PRODUCT,REC.${rid}`,
      "$ONEXIT.",
      "$IF,.NOT.FILE(REC,AS),GTRSYS.",
      "$  COMMON,SYSTEM.",
      `$  GTR,SYSTEM,REC.${rid}`,
      "$ENDIF,GTRSYS.",
      "$REWIND,REC.",
      "$COPYSBF,REC."
    ];
    if (typeof options === "undefined") options = {};
    options.jobname = "GTRSYS";
    if (typeof options.username === "undefined" && typeof options.user === "undefined") {
      options.username = "INSTALL";
      options.password = utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "INSTALL", "INSTALL");
    }
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

  getTlfTopology: dtc => {

    if (typeof utilities.tlfTopology !== "undefined") return utilities.tlfTopology;

    const spoolerTypes = {
      JES2: {
        mfType: "MVS",
        skipCount: 2
      },
      NOS: {
        mfType: "NOS2",
        skipCount: 2
      },
      PRIME: {
        mfType: "PRIME",
        skipCount: 0
      },
      RSCS: {
        mfType: "VM/CMS",
        skipCount: 2
      }
    };

    const customProps     = utilities.getCustomProperties(dtc);
    let   nextPort        = 0x28;
    let   portCount       = 8;
    utilities.tlfTopology = {};

    if (typeof customProps["NETWORK"] !== "undefined") {
      for (let line of customProps["NETWORK"]) {
        line = line.toUpperCase();
        let ei = line.indexOf("=");
        if (ei < 0) continue;
        let key   = line.substring(0, ei).trim();
        let value = line.substring(ei + 1).trim();
        if (key === "TLFNODE") {
          //
          //  tlfNode=<name>,<lid>,<spooler>,<addr>
          //    [,R<remote-id>][,P<password>][,B<block-size>]
          let items = value.split(",");
          if (items.length >= 4) {
            let node = {
              id: items.shift(),
              type: "TLF",
              lid: items.shift(),
              spooler: items.shift(),
              addr: items.shift(),
              remoteId: "",
              password: "",
              blockSize: 400
            };
            if (typeof spoolerTypes[node.spooler] === "undefined") {
              throw new Error(`Unrecognized TLF spooler type: ${node.spooler}`);
            }
            const spoolerInfo = spoolerTypes[node.spooler];
            for (const key of Object.keys(spoolerInfo)) node[key] = spoolerInfo[key];
            while (items.length > 0) {
              let item = items.shift();
              if (item.startsWith("B")) {
                node.blockSize = parseInt(item.substring(1));
              }
              else if (item.startsWith("P")) {
                node.password = item.substring(1);
              }
              else if (item.startsWith("R")) {
                node.remoteId = item.substring(1);
              }
            }
            utilities.tlfTopology[node.id] = node;
          }
        }
        else if (key === "TLFPORTS") {
          let items = value.split(",");
          if (items.length > 0) nextPort  = parseInt(items.shift());
          if (items.length > 0) portCount = parseInt(items.shift());
        }
      }
    }
    const nodeNames = Object.keys(utilities.tlfTopology).sort();
    for (const name of nodeNames) {
      if (portCount < 1) throw new Error("Insufficient number of TLF ports defined");
      utilities.tlfTopology[name].claPort = nextPort++;
      portCount -= 1;
    }
    return utilities.tlfTopology;
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

  purgeCache: () => {
    delete utilities.adjacentNjeNodes;
    delete utilities.customProperties;
    delete utilities.defaultNjeRoute;
    delete utilities.hostId;
    delete utilities.hosts;
    delete utilities.machineId;
    delete utilities.njeTopology;
    delete utilities.nonadjacentNjeNodesWithLids;
    delete utilities.nonadjacentNjeNodesWithoutLids;
    delete utilities.rhpTopology;
    delete utilities.timeZone;
    delete utilities.tlfTopology;
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
  },

  updateNDL: (dtc, modset) => {
    if (Array.isArray(modset)) {
      modset = `${modset.join("\n")}\n`;
    }
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
      jobname:  "UPDNDL",
      username: "NETADMN",
      password: utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "NETADMN", "NETADMN"),
      data:     modset
    };
    return dtc.createJobWithOutput(12, 4, job, options)
    .then(() => dtc.say("NDL updated"));
  }
};

module.exports = utilities;
