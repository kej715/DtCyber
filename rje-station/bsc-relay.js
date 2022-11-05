#!/usr/bin/env node
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2022, Kevin Jordan
**
**  bsc-relay.js
**    This program implements a relay for BSC communcation across slow or
**    unreliable network connections. The BSC/TCP protocol is sensitive to
**    network latency and reliability and doesn't operate well over networks
**    with high latency or low reliability. This program can operate on the
**    same host as an RJE station or RJE host and exchange BSC packets with
**    the station or host over reliable, 0-latency "localhost" connections.
**    It will relay the contents of these packets (e.g., HASP blocks) to
**    a peer that is communicating with a host or station on its end.
**    BSC ACK0 packets are exchanged and NAK packets are handled locally. They
**    are  not sent between peers.
**
**--------------------------------------------------------------------------
*/
const fs      = require("fs");
const net     = require("net");
const process = require("process");

const Bsc     = require("./bsc");
const Logger  = require("./logger");

const connectToPeer = nexus => {
  nexus.socket = net.createConnection({port:nexus.relay.port, host:nexus.relay.host}, () => {
    log(`${nexus.id} relay connection established to ${nexus.relay.host}:${nexus.relay.port}`);
    nexus.bsc.bsc = new Bsc({
      debug: isDebug,
      isStationRole: false,
      port: nexus.bsc.listen.port
    });
    start(nexus);
  });
  nexus.socket.on("error", err => {
    log(`${nexus.id} ${err}`);
    setTimeout(() => {
      connectToPeer(nexus);
    }, 5000);
  });
};

const listenForPeer = nexus => {
  nexus.server = net.createServer(socket => {
    log(`${nexus.id} relay connection established to ${socket.remoteAddress}`);
    nexus.socket = socket;
    nexus.bsc.bsc = new Bsc({
      debug: isDebug,
      isStationRole: true,
      port: nexus.bsc.connect.port,
      host: typeof nexus.bsc.connect.host === "string" ? nexus.bsc.connect.host : "127.0.0.1"
    });
    nexus.server.close();
    start(nexus);
  });
  nexus.server.on("error", err => {
    log(`${nexus.id} ${err}`);
    setTimeout(() => {
      listenForPeer(nexus);
    }, 5000);
  });
  nexus.server.listen(nexus.relay.port, () => {
    log(`${nexus.id} listening for peer connections on port ${nexus.relay.port}`);
  });
};

const log = msg => {
  logger.log(msg);
};

const logData = (label, data) => {
  if (isDebug) logger.logData(label, data);
};

const provideBlock = nexus => {
  if (nexus.receivedBlocks.length > 0) {
    return nexus.receivedBlocks.shift();
  }
  else {
    return null;
  }
};

const relayBlock = (nexus, data) => {
  nexus.socket.write(new Uint8Array([data.length >> 8, data.length & 0xff].concat(data)), () => {
    logData(`${nexus.id} sent:`, data);
  });
};

const start = nexus => {
  nexus.receivedBlocks = [];
  nexus.receivedData = [];
  nexus.bsc.bsc.setBlockProvider(() => {
    return provideBlock(nexus);
  });
  nexus.bsc.bsc.on("data", data => {
    relayBlock(nexus, data);
  });
  nexus.socket.on("data", data => {
    for (const b of data) nexus.receivedData.push(b);
    while (nexus.receivedData.length > 0) {
      if (nexus.receivedData.length < 2) break;
      let len = (nexus.receivedData[0] << 8) | nexus.receivedData[1];
      if (len + 2 > nexus.receivedData.length) break;
      let block = nexus.receivedData.slice(2, len + 2);
      nexus.receivedBlocks.push(block);
      logData(`${nexus.id} received:`, block);
      nexus.receivedData = nexus.receivedData.slice(len + 2);
    }
  });
  nexus.bsc.bsc.on("end", () => {
    log(`${nexus.id} BSC connection ended`);
    nexus.socket.end(() => {
      setTimeout(() => {
        if (typeof nexus.bsc.connect !== "undefined") {
          listenForPeer(nexus);
        }
        else {
          connectToPeer(nexus);
        }
      }, 5000);
    });
  });
  nexus.socket.on("end", () => {
    log(`${nexus.id} relay connection ended`);
    nexus.bsc.bsc.end();
    setTimeout(() => {
      if (typeof nexus.bsc.connect !== "undefined") {
        listenForPeer(nexus);
      }
      else {
        connectToPeer(nexus);
      }
    }, 5000);
  });
  nexus.bsc.bsc.start(() => {
    log(`${nexus.id} BSC connection started`);
  });
};

const configFile = (process.argv.length > 2) ? process.argv[2] : "bsc-relay.json";
const config = JSON.parse(fs.readFileSync(configFile));

const logger = new Logger("-");
const isDebug = typeof config.debug !== "undefined" ? config.debug : false;

for (const nexus of config.nexuses) {
  if (typeof nexus.bsc === "undefined") {
    process.stderr.write("bsc definition missing from configuration file entry\n");
    process.exit(1);
  }
  if (typeof nexus.relay === "undefined") {
    process.stderr.write("relay definition missing from configuration file entry\n");
    process.exit(1);
  }
  if (typeof nexus.relay.port === "undefined") {
    process.stderr.write("relay.port definition missing from configuration file entry\n");
    process.exit(1);
  }
  if (typeof nexus.bsc.listen !== "undefined") {
    if (typeof nexus.bsc.listen.port === "undefined") {
      process.stderr.write("bsc.listen.port definition missing from configuration file entry\n");
      process.exit(1);
    }
    if (typeof nexus.relay.host === "undefined") {
      process.stderr.write("relay.host definition missing from configuration file entry\n");
      process.exit(1);
    }
    connectToPeer(nexus);
  }
  else if (typeof nexus.bsc.connect !== "undefined") {
    if (typeof nexus.bsc.connect.port === "undefined") {
      process.stderr.write("bsc.connect.port definition missing from configuration file entry\n");
      process.exit(1);
    }
    listenForPeer(nexus);
  }
  else {
    process.stderr.write("bsc.listen or bsc.connect definition missing from configuration file entry\n");
    process.exit(1);
  }
}

