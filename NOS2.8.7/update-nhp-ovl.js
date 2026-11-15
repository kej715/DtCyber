#!/usr/bin/env node
//
// Update the cyber.ovl file to reflect the NHP configuration and RHP topology
//

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./opt/utilities");

const dtc = new DtCyber();

const hostID      = utilities.getHostId(dtc);
const config      = utilities.getNhpConfiguration(dtc);
const rhpTopology = utilities.getRhpTopology(dtc);

let configKeys = [];
for (const key of Object.keys(config)) {
  if (key.startsWith("MDI") || key.startsWith("NPU")) configKeys.push(key);
}
configKeys = configKeys.sort((k1, k2) => {
  return config[k1].couplerNode - config[k2].couplerNode;
});

const toHex = value => {
  return (value < 16 ? `0${value.toString(16)}` : value.toString(16)).toUpperCase();
};
const toOct = value => {
  return (value < 8 ? `0${value.toString(8)}` : value.toString(8)).toUpperCase();
};

let couplerMap  = {};
let hostAddrMap = {};
for (const node of rhpTopology) {
  couplerMap[`${node.id}.${node.npuNode}`] = node.couplerNode;
  hostAddrMap[node.id] = node.addr;
}

//
//  Update MDI and NPU definitions in equipment section
//
let props = {};
if (fs.existsSync("cyber.ovl")) {
  dtc.readPropertyFile("cyber.ovl", props);
}
let text = [];
if (typeof props["equipment.nos287"] !== "undefined") {
  for (const line of props["equipment.nos287"]) {
    let ci = line.indexOf(",");
    if (ci < 0) continue;
    let key   = line.substring(0, ci).trim().toUpperCase();
    let value = line.substring(ci + 1).trim();
    if (key !== "MDI" && key !== "NPU") {
       text.push(line);
    }
  }
}
for (const key of configKeys) {
  const netDevice = config[key];
  text.push(`${key.substring(0, 3)},${netDevice.eqNo.toString(8)},0,${toOct(netDevice.channelNo)},${key}.cfg`);
}
props["equipment.nos287"] = text;

//
//  Create configuration section for each MDI and NPU
//
for (const key of configKeys) {
  const netDevice = config[key];
  let text = [];
  text.push(`hostID=${hostID}`);
  text.push(`couplerNode=${netDevice.couplerNode}`);
  text.push(`npuNode=${netDevice.npuNode}`);
  if (key === "MDI0") {
    text.push(`cdcnetNode=${netDevice.cdcnetNode}`);
    if (typeof netDevice.cdcnetPrivilegedTcpPortOffset !== "undefined") {
      text.push(`cdcnetPrivilegedTcpPortOffset=${netDevice.cdcnetPrivilegedTcpPortOffset}`);
    }
    if (typeof netDevice.cdcnetPrivilegedUdpPortOffset !== "undefined") {
      text.push(`cdcnetPrivilegedUdpPortOffset=${netDevice.cdcnetPrivilegedUdpPortOffset}`);
    }
  }
  //
  //  Create definitions for trunks, if any
  //
  for (const node of rhpTopology) {
    if (node.id === hostID && node.npuNode === netDevice.npuNode) {
      let port = parseInt(node.addr.substring(node.addr.indexOf(":") + 1));
      for (const peerName of Object.keys(node.links)) {
        let peer = node.links[peerName];
        let couplerNode = couplerMap[`${peerName}.${peer.npuNode}`];
        text.push(`terminals=${port},0x${toHex(peer.localClaPort)},1,trunk,${hostAddrMap[peerName]},${peerName},${couplerNode}`);
      }
    }
  }
  //
  //  Create definitions for async printers, if any
  //
  if (typeof netDevice.raw !== "undefined") {
    for (const termDef of netDevice.raw) {
      text.push(`terminals=${termDef.tcpPort},0x${toHex(termDef.firstClaPort)},${termDef.claPortCount},raw`);
    }
  }
  //
  //  Create definitions for async terminals, if any
  //
  if (typeof netDevice.telnet !== "undefined") {
    for (const termDef of netDevice.telnet) {
      text.push(`terminals=${termDef.tcpPort},0x${toHex(termDef.firstClaPort)},${termDef.claPortCount},telnet`);
    }
  }
  //
  //  Create definitions for pterm terminals, if any
  //
  if (typeof netDevice.pterm !== "undefined") {
    for (const termDef of netDevice.pterm) {
      text.push(`terminals=${termDef.tcpPort},0x${toHex(termDef.firstClaPort)},${termDef.claPortCount},pterm`);
    }
  }
  //
  //  Create definitions for hasp terminals, if any
  //
  if (typeof netDevice.hasp !== "undefined") {
    let terminals = netDevice.hasp;
    let i         = 0;
    while (i < terminals.length) {
      let terminal = terminals[i++];
      let claPort  = terminal.firstClaPort;
      if (typeof terminal.blockSize === "undefined") terminal.blockSize = 400;
      let count    = 1;
      while (i < terminals.length) {
        let nextTerminal = terminals[i];
        if (typeof nextTerminal.blockSize === "undefined") nextTerminal.blockSize = 400;
        if (terminal.tcpPort === nextTerminal.tcpPort
            && (claPort + 1) === nextTerminal.firstClaPort
            && terminal.blockSize === nextTerminal.blockSize) {
          claPort += 1;
          count   += 1;
          i       += 1;
        }
        else {
          break;
        }
      }
      text.push(`terminals=${terminal.tcpPort},0x${toHex(terminal.firstClaPort)},${count},hasp,B${terminal.blockSize}`);
    }
  }
  //
  //  Create definitions for reverse hasp terminals, if any
  //
  if (typeof netDevice.tlf !== "undefined") {
    for (const termDef of netDevice.tlf) {
      text.push(`terminals=0,0x${toHex(termDef.firstClaPort)},${termDef.claPortCount},rhasp,${termDef.addr},B${termDef.blockSize}`);
    }
  }
  //
  //  Create definitions for NJE terminals, if any
  //
  if (typeof netDevice.nje !== "undefined") {
    const njeTopology  = utilities.getNjeTopology(dtc);
    let localNode      = njeTopology[hostID];
    let defaultLocalIP = null;
    if (typeof localNode.publicAddress !== "undefined") {
      let parts = localNode.publicAddress.split(":");
      if (parts[0] !== "0.0.0.0") {
        defaultLocalIP = parts[0];
      }
    }
    for (const termDef of netDevice.nje) {
      let tcpPort = 175;
      let localIP = defaultLocalIP;
      if (typeof termDef.localAddress !== "undefined") {
        let parts = termDef.localAddress.split(":");
        localIP   = parts[0];
        if (parts.length > 1) {
          tcpPort = parts[1];
        }
      }
      let defn = `terminals=${tcpPort},0x${toHex(termDef.firstClaPort)},${termDef.claPortCount},nje,${termDef.publicAddress},${termDef.id}`;
      if (typeof termDef.localAddress !== "undefined") {
        let parts = termDef.localAddress.split(":");
        defn += `,${parts[0]}`;
      }
      else if (localIP !== null && localIP !== "0.0.0.0") {
        defn += `,${localIP}`;
      }
      if (typeof termDef.blockSize !== "undefined") {
        defn += `,B${termDef.blockSize}`;
      }
      if (typeof termDef.pingInterval !== "undefined") {
        defn += `,P${termDef.pingInterval}`;
      }
      text.push(defn);
    }
  }
  props[`${key}.cfg`] = text;
}

utilities.writePropertyFile("cyber.ovl", props);
