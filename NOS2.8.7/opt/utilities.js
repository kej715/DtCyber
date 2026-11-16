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

  defaultNhpConfiguration: {
    isDefault: true,
    hostId: "NCCM01",
    MDI0: {
      id: "MDI0",
      channelNo: 5,
      eqNo: 7,
      pipNo: 1,
      couplerNode: 1,
      npuNode: 2,
      cdcnetNode: 255,
      cdcnetPrivilegedTcpPortOffset: 0,
      cdcnetPrivilegedUdpPortOffset: 0,
      telnet: [
        {
        firstClaPort: 0x04,
        claPortCount: 16,
        tcpPort: 23,
        user: "IAFUSER"
        }
      ],
      pterm: [
        {
        firstClaPort: 0x14,
        claPortCount: 16,
        tcpPort: 8005,
        user: "CYBUSER"
        }
      ],
      hasp: [
        {
        firstClaPort: 0x24,
        claPortCount: 1,
        tcpPort: 2553,
        termClass: "HPRE", // default
        blockSize: 400,    // default
        user: "RBFUSER,MUSER=RJE1"
        },
        {
        firstClaPort: 0x25,
        claPortCount: 1,
        tcpPort: 2554,
        termClass: "HPRE", // default
        blockSize: 400,    // default
        user: "RBFUSER,MUSER=RJE2"
        }
      ],
      printers: [
        {
        firstClaPort: 0x03,
        claPortCount: 1,
        tcpPort: 2552,
        user: "MFAM=CYBER,MUSER=PRINT05,MAPPL=PSU"
        }
      ]
    }
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

  editCmrdProps: (cmrDeck, props) => {
    for (const prop of props) {
      let ei = prop.indexOf("=");
      if (ei < 0) {
        throw new Error(`Invalid CMRDECK definition: \"${prop}\"`);
      }
      let key   = prop.substring(0, ei).trim().toUpperCase();
      let value = prop.substring(ei + 1).trim().toUpperCase();
      if (value.endsWith(".")) value = value.substring(0, value.length - 1).trim();
      let si = 0;
      while (si < cmrDeck.length) {
        let ni = cmrDeck.indexOf("\n", si);
        if (ni < 0) ni = cmrDeck.length - 1;
        let ei = cmrDeck.indexOf("=", si);
        if (ei < ni && ei > 0 && cmrDeck.substring(si, ei).trim() === key) {
          if (value !== "") {
            cmrDeck = `${cmrDeck.substring(0, si)}${key}=${value}.\n${cmrDeck.substring(ni + 1)}`;
          }
          else {
            cmrDeck = `${cmrDeck.substring(0, si)}${cmrDeck.substring(ni + 1)}`;
          }
          break;
        }
        si = ni + 1;
      }
      if (si >= cmrDeck.length) {
        cmrDeck += `${key}=${value}.\n`;
      }
    }
    return cmrDeck;
  },

  editEqpdProps: (eqpDeck, props) => {
    for (const prop of props) {
      let ei = prop.indexOf("=");
      if (ei < 0) {
        ei = prop.indexOf(",");
        if (ei < 0) {
          ei = prop.indexOf(".");
          if (ei < 0) {
            throw new Error(`Invalid EQPDECK definition: \"${prop}\"`);
          }
        }
      }
      let key   = prop.substring(0, ei).trim().toUpperCase();
      let value = prop.substring(ei + 1).trim().toUpperCase();
      let si = 0;
      let isEQyet = false;
      let isPFyet = false;
      while (si < eqpDeck.length) {
        let ni = eqpDeck.indexOf("\n", si);
        if (ni < 0) ni = eqpDeck.length - 1;
        let ei = eqpDeck.indexOf("=", si);
        if (ei < ni && ei > 0) {
          let eqpdKey = eqpDeck.substring(si, ei).trim();
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
              ci = eqpDeck.indexOf(",", ei + 1);
              let eqpdPFN = parseInt(eqpDeck.substring(ei + 1, ci).trim());
              if (propPFN === eqpdPFN) {
                eqpDeck = `${eqpDeck.substring(0, si)}${key}=${value}\n${eqpDeck.substring(ni + 1)}`;
                break;
              }
              else if (propPFN < eqpdPFN) {
                eqpDeck = `${eqpDeck.substring(0, si)}${key}=${value}\n${eqpDeck.substring(si)}`;
                break;
              }
            }
            else if (value === "" || value === ".") {
              eqpDeck = `${eqpDeck.substring(0, si)}${eqpDeck.substring(ni + 1)}`;
              break;
            }
            else {
              eqpDeck = `${eqpDeck.substring(0, si)}${key}=${value}\n${eqpDeck.substring(ni + 1)}`;
              break;
            }
          }
          else if (isEQyet && key.startsWith("EQ") && !eqpdKey.startsWith("*")) {
            if (!eqpdKey.startsWith("EQ")
                || parseInt(key.substring(2)) < parseInt(eqpdKey.substring(2))) {
              eqpDeck = `${eqpDeck.substring(0, si)}${key}=${value}\n${eqpDeck.substring(si)}`;
              break;
            }
          }
          else if (isPFyet && key === "PF" && !eqpdKey.startsWith("*") && eqpdKey !== "REMOVE") {
            eqpDeck = `${eqpDeck.substring(0, si)}${key}=${value}\n${eqpDeck.substring(si)}`;
            break;
          }
        }
        else {
          let eqpdProp = eqpDeck.substring(si, ni);
          if (eqpdProp.startsWith(key)) {
            eqpDeck = `${eqpDeck.substring(0, si)}${prop.toUpperCase()}\n${eqpDeck.substring(ni + 1)}`;
            break;
          }
        }
        si = ni + 1;
      }
      if (si >= eqpDeck.length) {
        eqpDeck += `${prop.toUpperCase()}\n`;
      }
    }
    return eqpDeck;
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

  getCouplerNode: (dtc, devId) => {
    const nhpConfig = utilities.getNhpConfiguration(dtc);
    return rhpConfig[devId].couplerNode;
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
        const hostID    = utilities.getHostId(dtc);
        const localNode = utilities.njeTopology[hostID];
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
    const config = utilities.getNhpConfiguration(dtc);
    utilities.hostId = (typeof config.hostId !== "undefined" && config.hostId !== null) ? config.hostId : `M${utilities.getMachineId(dtc)}`;
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

  getNhpConfiguration: dtc => {
    if (typeof utilities.nhpConfiguration !== "undefined") return utilities.nhpConfiguration;
    //
    //  Initialize with default NOS 2.8.7 configuration from ready-to-run packages
    //
    let config = utilities.defaultNhpConfiguration;
    const customProps = utilities.getCustomProperties(dtc);
    if (typeof customProps["NETWORK"] === "undefined") return config;
    //
    //  First, scan the NETWORK section. If an MDI/NPU declaration is discovered before any MDI/NPU
    //  parameter definitions, discard the default configuration, and start from scratch. Otherwise,
    //  MDI/NPU parameter definitions will be added/updated in the default MDI0 configuration.
    //
    let mdiNo     = 0;
    let npuNo     = 0;
    let netDevice = config["MDI0"];
    for (let line of customProps["NETWORK"]) {
      let ei = line.indexOf("=");
      if (ei < 0) continue;
      let key = line.substring(0, ei).trim().toUpperCase();
      if (key === "MDI" || key === "NPU") {
        config = {
          isDefault: false,
          hostId: utilities.defaultNhpConfiguration.hostId
        };
        netDevice = {};
        break;
      }
      else if (key === "CRAYSTATION"
            || key === "DEFAULTROUTE"
            || key === "RHPNODE"
            || key === "SAFNODE"
            || key === "STKDRIVEPATH") {
        continue;
      }
      else if (key === "HOSTID") {
        config.isDefault = false;
      }
      else {
        config.isDefault = false;
        mdiNo += 1;
        break;
      }
    }
    //
    //  Next, parse the whole NETWORK section and create/update the configuration
    //  from it.
    //
    let nextHaspClaPort = 0;
    let haspPortCount   = 0;
    let nextNjeClaPort  = 0;
    let njePortCount    = 0;
    let nextTlfClaPort  = 0;
    let tlfPortCount    = 0;
    let node            = {};
    for (let line of customProps["NETWORK"]) {
      line   = line.toUpperCase();
      let ei = line.indexOf("=");
      if (ei < 0) continue;
      let key    = line.substring(0, ei).trim();
      let params = line.substring(ei + 1).trim().split(",");
      if (params.length < 1) {
        throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
      }
      switch (key) {

      default:
        // ignore and do nothing
        break;

      case "ASYNCPORTS": // asyncPorts=<claPort>,<portCount>,<tcpPort>[,<lcfUser>]
        if (params.length < 3) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (typeof netDevice.telnet === "undefined") netDevice.telnet = [];
        netDevice.telnet.push(
          {
          firstClaPort: parseInt(params[0]),
          claPortCount: parseInt(params[1]),
          tcpPort: parseInt(params[2]),
          user: params.length > 3 ? params.slice(3).join(",") : "IAFUSER"
          }
        );
        break;

      case "CDCNETNODE":
        netDevice.cdcnetNode = parseInt(params[0]);
        break;

      case "CDCNETPRIVILEGEDTCPPORTOFFSET":
        netDevice.cdcnetPrivilegedTcpPortOffset = parseInt(params[0]);
        break;

      case "CDCNETPRIVILEGEDUDPPORTOFFSET":
        netDevice.cdcnetPrivilegedUdpPortOffset = parseInt(params[0]);
        break;

      case "COUPLERNODE":
        netDevice.couplerNode = parseInt(params[0]);
        break;

      case "CYBISPORTS": // cybisPorts=<claPort>,<portCount>,<tcpPort>[,<lcfUser>]
        if (params.length < 3) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (typeof netDevice.pterm === "undefined") netDevice.pterm = [];
        netDevice.pterm.push(
          {
          firstClaPort: parseInt(params[0]),
          claPortCount: parseInt(params[1]),
          tcpPort: parseInt(params[2]),
          user: params.length > 3 ? params.slice(3).join(",") : "CYBUSER"
          }
        );
        break;

      case "HASPPORTS": // haspPorts=<claPort>,<portCount>
        if (params.length < 2) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (typeof netDevice.hasp === "undefined") netDevice.hasp = [];
        nextHaspClaPort = parseInt(params[0]);
        haspPortCount   = parseInt(params[1]);
        break;

      case "HASPTERMINAL": // haspTerminal=<name>,<tcpPort>,[,B<blockSize>][,T<terminalClass>]
        if (params.length < 2) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (haspPortCount < 1) {
          throw new Error(`No ports available for ${key} definition in NETWORK section of site.cfg`);
        }
        let blockSize = 400;
        let termClass = "HPRE";
        for (let i = 2; i < params.length; i++) {
          if (params[i].startsWith("B")) {
            blockSize = parseInt(params[i].substring(1));
          }
          else if (params[i].startsWith("T")) {
            termclass = params[i].substring(1); 
          }
        }
        netDevice.hasp.push(
          {
          firstClaPort: nextHaspClaPort++,
          claPortCount: 1,
          tcpPort: parseInt(params[1]),
          termClass: termClass,
          blockSize: blockSize,
          user: `RBFUSER,MUSER=${params[0]}`
          }
        );
        haspPortCount -= 1;
        break;

      case "HOSTID":
        config.hostId = params[0];
        break;

      case "MDI": // MDI=<couplerNode>,<npuNode>,<channelNo>[,E<eqNo>][,P<pipNo>]
      case "NPU": // NPU=<couplerNode>,<npuNode>,<channelNo>[,E<eqNo>][,P<pipNo>]
        if (params.length < 3) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        let devName = `${key}${key === "MDI" ? mdiNo++ : npuNo++}`;
        if (typeof config[devName] === "undefined") config[devName] = {};
        netDevice             = config[devName];
        netDevice.id          = devName;
        netDevice.couplerNode = parseInt(params[0]);
        netDevice.npuNode     = parseInt(params[1]);
        netDevice.channelNo   = parseInt(params[2], 8);
        netDevice.eqNo        = 7;
        netDevice.pipNo       = Math.floor((mdiNo + npuNo - 1) / 2) + 1;
        for (const param of params.slice(3)) {
          if (param.startsWith("E")) {
            netDevice.eqNo = parseInt(param.substring(1), 8);
          }
          else if (param.startsWith("P")) {
            netDevice.pipNo = parseInt(param.substring(1));
          }
        }
        if (devName === "MDI0") {
          netDevice.cdcnetNode = 255;
        }
        nextHaspClaPort = 0;
        haspPortCount   = 0;
        nextNjeClaPort  = 0;
        njePortCount    = 0;
        nextTlfClaPort  = 0;
        tlfPortCount    = 0;
        break;

      case "NJENODE": // njeNode=<nodename>,<software>,<lid>,<public-addr>,<link>
                      //     [,<local-address>][,B<block-size>][,P<ping-interval>][,<mailer-address>]
        if (params.length < 5) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (njePortCount < 1) {
          throw new Error(`No ports available for ${key} definition in NETWORK section of site.cfg`);
        }
        node = {
          id: params[0],
          netDevice: netDevice,
          firstClaPort: nextNjeClaPort++,
          claPortCount: 1,
          type: "NJE",
          software: params[1],
          lid: params[2],
          publicAddress: params[3],
          link: params[4],
          user: "NJFUSER"
        };
        if (params.length > 5) {
          node.localAddress = params[5];
        }
        for (let i = 6; i < params.length; i++) {
          if (params[i].startsWith("B")) {
            node.blockSize = parseInt(params[i].substring(1));
          }
          else if (params[i].startsWith("P")) {
            node.pingInterval = parseInt(params[i].substring(1));
          }
          else if (params[i].indexOf("@") > 0) {
            node.mailer = params[i];
          }
        }
        netDevice.nje.push(node);
        njePortCount -= 1;
        break;

      case "NJEPORTS": // njePorts=<claPort>,<portCount>
        if (params.length < 2) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (typeof netDevice.nje === "undefined") netDevice.nje = [];
        nextNjeClaPort = params[0];
        njePortCount   = params[1];
        break;

      case "NPUNODE":
        netDevice.npuNode = parseInt(params[0]);
        break;

      case "PRINTERPORTS": // printerPorts=<claPort>,<portCount>,<tcpPort>[,<lcfUser>]
        if (params.length < 3) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (typeof netDevice.raw === "undefined") netDevice.raw = [];
        netDevice.raw.push(
          {
          firstClaPort: parseInt(params[0]),
          claPortCount: parseInt(params[1]),
          tcpPort: parseInt(params[2]),
          user: params.length > 3 ? params.slice(3).join(",") : "MFAM=CYBER,MUSER=PRINT05,MAPPL=PSU"
          }
        );
        break;

      case "TLFNODE": // tlfNode=<name>,<lid>,<spooler>,<addr>
          //    [,R<remote-id>][,P<password>][,B<block-size>]
        if (params.length < 4) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (tlfPortCount < 1) {
          throw new Error(`No ports available for ${key} definition in NETWORK section of site.cfg`);
        }
        node = {
          id: params[0],
          netDevice: netDevice,
          firstClaPort: nextTlfClaPort++,
          claPortCount: 1,
          type: "TLF",
          lid: params[1],
          spooler: params[2],
          addr: params[3],
          remoteId: "",
          password: "",
          blockSize: 400,
          user: "TLFUSER"
        };
        if (typeof utilities.tlfSpoolerTypes[node.spooler] === "undefined") {
          throw new Error(`Unrecognized TLF spooler type: ${node.spooler}`);
        }
        const spoolerInfo = utilities.tlfSpoolerTypes[node.spooler];
        for (const key of Object.keys(spoolerInfo)) node[key] = spoolerInfo[key];
        for (let i = 4; i < params.length; i++) {
          if (params[i].startsWith("B")) {
            node.blockSize = parseInt(params[i].substring(1));
          }
          else if (params[i].startsWith("P")) {
            node.password = params[i].substring(1);
          }
          else if (params[i].startsWith("R")) {
            node.remoteId = params[i].substring(1);
          }
        }
        netDevice.tlf.push(node);
        tlfPortCount -= 1;
        break;

      case "TLFPORTS": // tlfPorts=<claPort>,<portCount>
        if (params.length < 2) {
          throw new Error(`${key} definition in NETWORK section of site.cfg has too few parameters`);
        }
        if (typeof netDevice.tlf === "undefined") netDevice.tlf = [];
        nextTlfClaPort = params[0];
        tlfPortCount   = params[1];
        break;
      }
    }
    utilities.nhpConfiguration = config;
    return config;
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
 
      let njeTopology = utilities.njeTopology;

      const nhpConfig = utilities.getNhpConfiguration(dtc);
      for (const devName of Object.keys(nhpConfig)) {
        if (typeof nhpConfig[devName].nje !== "undefined") {
          for (const node of nhpConfig[devName].nje) {
            njeTopology[node.id] = node;
          }
        }
      }

      const hostID = utilities.getHostId(dtc);
      const mid    = utilities.getMachineId(dtc);

      utilities.adjacentNjeNodes               = {};
      utilities.nonadjacentNjeNodesWithLids    = {};
      utilities.nonadjacentNjeNodesWithoutLids = {};

      if (typeof utilities.defaultNjeRoute === "undefined") {
        utilities.defaultNjeRoute = utilities.getPropertyValue(utilities.getCustomProperties(dtc), "NETWORK", "defaultRoute", null);
      }
      if (typeof njeTopology[hostID] === "undefined") {
        njeTopology[hostID] = {
          id: hostID,
          type: "NJE",
          software: "NJEF",
          lid: `M${mid}`
        };
        if (utilities.defaultNjeRoute !== null) njeTopology[hostID].link = utilities.defaultNjeRoute;
      }
      if (typeof njeTopology[hostID].link !== "undefined") {
        let routingNode = njeTopology[njeTopology[hostID].link];
        utilities.adjacentNjeNodes[njeTopology[hostID].link] = routingNode;
      }
      for (const key of Object.keys(njeTopology).sort()) {
        let node = njeTopology[key];
        if (node.link === hostID) {
          utilities.adjacentNjeNodes[key] = node;
        }
      }
      for (const key of Object.keys(njeTopology)) {
        let node = njeTopology[key];
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

  getNpuNode: (dtc, devId) => {
    const config = utilities.getNhpConfiguration(dtc);
    return config[devId].npuNode;
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
    utilities.rhpTopology = [];
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
          //  rhpNode=<host-id>,<lid>,<addr>,<coupler-node>,<npu-node>,<remote-host-id>,<npu-node>,<local-cla-port>
          //    [,<remote-host-id>,<npu-node>,<local-cla-port>...]
          //
          let items = value.split(",");
          if (items.length < 9) {
            throw new Error("Invalid rhpNode definition: at least 9 parameters required");
          }
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
          while (items.length >= 3) {
            let linkName         = items.shift();
            let link             = {};
            link.id              = linkName;
            link.npuNode         = parseInt(items.shift());
            link.localClaPort    = parseInt(items.shift());
            node.links[linkName] = link;
          };
          if (items.length > 0) {
            throw new Error(`Invalid rhpNode definition: NPU node number, local CLA port, and/or remote CLA port missing for link ${items.shift()}`);
          }
          utilities.rhpTopology.push(node);
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
    if (typeof options.jobname === "undefined") options.jobname = rid;
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
    if (typeof utilities.tlfTopology === "undefined") {
      const config = utilities.getNhpConfiguration(dtc);
      utilities.tlfTopology = {};
      for (const key of Object.keys(config)) {
        if (key.startsWith("MDI") || key.startsWith("NPU")) {
          let netDevice = config[key];
          if (typeof netDevice.tlf !== "undefined") {
            for (const node of netDevice.tlf) {
              utilities.tlfTopology[node.id] = node;
            }
          }
        }
      }
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

  putPropertyValue: (propertyObject, sectionName, propertyName, propertyValue) => {
    const ucPropName = propertyName.toUpperCase();
    let propList = [];
    if (typeof propertyObject[sectionName] === "undefined") {
      propertyObject[sectionName] = [];
    }
    let found = false;
    for (const line of propertyObject[sectionName]) {
      let ei = line.indexOf("=");
      if (ei === -1) continue;
      let key = line.substring(0, ei).trim().toUpperCase();
      if (key === ucPropName) {
        propList.push(`${propertyName}=${propertyValue}`);
        found = true;
      }
      else {
        propList.push(line);
      }
    }
    if (!found) {
      propList.push(`${propertyName}=${propertyValue}`);
    }
    propertyObject[sectionName] = propList;
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

  tlfSpoolerTypes: {
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
  },

  updateNDL: (dtc, modset) => {
    if (Array.isArray(modset)) {
      modset = `${modset.join("\n")}\n`;
    }
    const job = [
      "$COPY,INPUT,NDLMODS.",
      "$REPLACE,NDLMODS."
    ];
    const options = {
      jobname:  "UPDNDL",
      username: "NETADMN",
      password: utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "NETADMN", "NETADMN"),
      data:     modset
    };
    return dtc.say("Update NDL ...")
    .then(() => dtc.createJobWithOutput(12, 4, job, options))
    .then(() => dtc.say("NDL updated"));
  },

  updateProductRecords: (dtc, productRecords) => {
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
      password: utilities.getPropertyValue(utilities.getCustomProperties(dtc), "PASSWORDS", "INSTALL", "INSTALL"),
      data:    `${productRecords.join("~eor\n")}`
    };
    return dtc.say("Update PRODUCT ...")
    .then(() => dtc.createJobWithOutput(12, 4, job, options))
    .then(output => {
      let promise = Promise.resolve();
      for (const line of output.split("\n")) {
        promise = promise
        .then(() => dtc.say(`${new Date().toLocaleTimeString()}   ${line}`));
      }
      return promise;
    });
  },

  writePropertyFile: (pathname, props) => {
    if (typeof pathname !== "string") {
      props = pathname;
      pathname = "site.cfg";
    }
    let lines = [];
    for (const sectionKey of Object.keys(props)) {
      let propList = props[sectionKey];
      lines.push(`[${sectionKey}]`);
      for (const line of propList) {
        lines.push(line);
      }
    }
    fs.writeFileSync(pathname, lines.join("\n") + "\n");
  }

};

module.exports = utilities;
