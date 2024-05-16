#!/usr/bin/env node
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2022, Kevin Jordan
**
**  rjews.js
**    This program implements a web service for remote job entry.
**    It interacts with mainframe emulators such as DtCyber and Hercules
**    and emulates an RJE station.
**
**--------------------------------------------------------------------------
*/
const fs = require("fs");
const http = require("http");
const net = require("net");
const os = require('node:os');
const path = require("path");
const process = require("process");
const Readable = require('stream').Readable;
const url = require("url");
const WebSocketServer = require("websocket").server;

const Hasp = require("./hasp");
const Mode4 = require("./mode4");
const RJE = require("./rje");

const VERSION = "1.1";

let connections = [];
let config = {};
let machineMap = {};

const mediaTypeMap = {
  "bmp" : "image/bmp",
  "css" : "text/css",
  "html": "text/html",
  "jpg" : "image/jpeg",
  "js"  : "text/javascript",
  "json": "application/json",
  "png" : "image/png",
  "txt" : "text/plain"
};

const streamTypes = [
  "CO",
  "CR",
  "LP",
  "CP"
];

class ReaderStream extends Readable {

  constructor(options) {
    super(options);
  }

  _read(n) {
  }
}

const generateDefaultResponse = (req, res) => {
  res.writeHead(200, {"Content-Type":"text/html"});
  res.write("<html>");
  res.write(  "<head>");
  res.write(    '<link rel="stylesheet" href="css/styles.css" />');
  res.write(    '<link rel="icon" type="image/png" href="images/ncc.png" />');
  res.write(    '<script src="js/jquery-3.3.1.min.js"></script>');
  res.write(  "</head>");
  res.write(  "<body>");
  res.write(    "<h1>Available RJE Services</h1>");
  res.write(    '<ul id="machines" class="machine-list">');

  for (const key of Object.keys(machineMap)) {
    let machine = machineMap[key];
    res.write(`<li><a href="rje.html?m=${machine.id}&t=${encodeURIComponent(machine.title)}" target="_blank">${key}</a> : ${machine.title}</li>`);
  }

  res.write("</ul></body></html>");
  res.end();
  logHttpRequest(req, 200);
}

const getAvailableConnection = () => {

  for (let connection of connections) {
    if (connection.isConnected === false) return connection;
  }

  let newConnection = {
    id: connections.length,
    isConnected: false
  }

  connections.push(newConnection);

  return newConnection;
};

const log = msg => {
  console.log(`${new Date().toLocaleString()} ${msg}`);
};

const logHttpRequest = (req, status) => {
  log(`http ${req.socket.remoteAddress} ${status} ${req.method} ${req.url}`);
};

const logWsRequest = msg => {
  log(`ws   ${msg}`);
};

const processMachineRequest = (req, res, path) => {
  let machine = machineMap[path.substring(path.lastIndexOf("/") + 1)];
  if (machine) {
    res.writeHead(200, {"Content-Type":"application/json"});
    res.write(JSON.stringify(machine));
    res.end();
    logHttpRequest(req, 200);
  }
  else {
    res.writeHead(400);
    res.end();
    logHttpRequest(req, 400);
  }
};

const processMachinesRequest = (req, res, query) => {
  if (query.machine) {
    let machine = machineMap[query.machine];
    if (machine) {
      let connection = getAvailableConnection();
      connection.machineName = query.machine;
      delete connection.ws;
      connection.streams = {};
      connection.service = null;
      connection.idleTimeout = (typeof machine.idleTimeout === "number")
        ? machine.idleTimeout
        : 10 * 60 * 1000;

      switch (machine.protocol) {
      case "hasp":
        connection.service = new Hasp(machine);
        break;
      case "mode4":
        connection.service = new Mode4(machine);
        break;
      default:
        log(`Unrecognized RJE protocol: ${machine.protocol} in ${key}`);
        process.exit(1);
      }

      connection.service.on("connect", () => {
        log(`Connected to ${machine.protocol} service on ${connection.machineName} at ${machine.host}:${machine.port}`);
        connection.isConnected = true;
        connection.lastAction = Date.now();
        res.writeHead(200, {"Content-Type":"text/plain"});
        res.end(connection.id.toString());
        machine.curConnections += 1;
        logHttpRequest(req, 200);
      });

      connection.service.on("data", (streamType, streamId, data) => {
        const key = `${streamTypes[streamType]}${streamId}`;
        if (typeof connection.streams[key] === "undefined") {
          connection.streams[key] = {};
          connection.streams[key].type = streamType;
          connection.streams[key].id   = streamId;
          connection.streams[key].data = "";
        }
        const stream = connection.streams[key];
        const eol = streamType === RJE.StreamType_Console ? "\n" : "";
        if (data !== null) {
          stream.data += `${data}${eol}`;
          stream.isEOI = false;
        }
        else {
          stream.isEOI = true;
        }
        if (connection.ws) {
          if (stream.data.length > 0) {
            sendData(connection, streamType, streamId, `${stream.data}`);
            stream.data = "";
          }
          if (stream.isEOI) {
            connection.ws.sendUTF(`${streamType} ${streamId} 0 `);
          }
        }
        connection.lastAction = Date.now();
      });

      connection.service.on("end", streamId => {
        const key = `${streamTypes[RJE.StreamType_Reader]}${streamId}`;
        if (typeof connection.streams[key] !== "undefined") {
          const stream = connection.streams[key];
          stream.isReady = false;
          delete stream.stream;
        }
        machine.curConnections -= 1;
        log(`${machine.id} : ${key} done`);
      });

      connection.service.on("error", err => {
        log(`${machine.id} : ${err}`);
        connection.service.end();
        if (typeof connection.ws !== "undefined") {
          connection.ws.close();
          delete connection.ws;
        }
        else if (connection.isConnected === false) {
          res.writeHead(500, {"Content-Type":"text/plain"});
          res.end(err.toString());
          logHttpRequest(req, 500);
        }
        connection.isConnected = false;
        machine.curConnections -= 1;
      });

      connection.service.on("pti", (streamType, streamId) => {
        const key = `${streamTypes[streamType]}${streamId}`;
        if (typeof connection.streams[key] !== "undefined") {
          const stream = connection.streams[key];
          if (stream.isReady === false) {
            log(`${machine.id} : ${key} ready to receive`);
          }
          stream.isReady = true;
          if (typeof stream.stream !== "undefined") {
            connection.service.send(streamId, stream.stream);
          }
        }
      });

      connection.service.start(
        (typeof machine.name     !== "undefined") ? machine.name     : null,
        (typeof machine.password !== "undefined") ? machine.password : null
      );
    }
    else {
      res.writeHead(404);
      res.end();
      logHttpRequest(req, 404);
    }
  }
  else if (Object.keys(query).length < 1) {
    res.writeHead(200, {"Content-Type":"application/json"});
    res.end(JSON.stringify(machineMap));
  }
  else {
    res.writeHead(400);
    res.end();
    logHttpRequest(req, 400);
  }
};

const processReceivedData = (connection, data) => {
  if (typeof data === "object") {
    for (let i = 0; i < data.length; i++) connection.inputBuffer += String.fromCharCode(data[i]);
  }
  else {
    connection.inputBuffer += data;
  }
  while (connection.inputBuffer.length > 0) {
    //
    // Parse stream type
    //
    let fi = 0;
    while (fi < connection.inputBuffer.length && /\s/.test(connection.inputBuffer.charAt(fi))) fi += 1;
    if (fi >= connection.inputBuffer.length) return;
    let si = connection.inputBuffer.indexOf(" ", fi);
    if (si < 0) return;
    const streamType = parseInt(connection.inputBuffer.substring(fi, si));
    //
    // Parse stream identifier
    //
    fi = si + 1;
    si = connection.inputBuffer.indexOf(" ", fi);
    if (si < 0) return;
    const streamId = parseInt(connection.inputBuffer.substring(fi, si));
    //
    // Parse record length
    //
    fi = si + 1;
    si = connection.inputBuffer.indexOf(" ", fi);
    if (si < 0) return;
    const len = parseInt(connection.inputBuffer.substring(fi, si));
    fi = si + 1;
    if (fi + len > connection.inputBuffer.length) return;
    let text = connection.inputBuffer.substring(fi, fi + len);
    connection.inputBuffer = connection.inputBuffer.substring(fi + len);
    //
    // Handle stream-specific data
    //
    let key = `${streamTypes[streamType]}${streamId}`;
    if (typeof connection.streams[key] === "undefined") {
      connection.streams[key] = {};
      connection.streams[key].type = streamType;
      connection.streams[key].id   = streamId;
      connection.streams[key].data = "";
    }
    let stream = connection.streams[key];
    if (len === 0) stream.isEOI = true;

    if (typeof connection.service !== "undefined") {
      switch (streamType) {
      case RJE.StreamType_Console:
        connection.service.command(text.trim());
        break;
      case RJE.StreamType_Reader:
        if (typeof stream.stream === "undefined") {
          stream.stream = new ReaderStream();
          connection.service.requestToSend(streamId);
        }
        if (stream.isEOI) {
          stream.stream.push(null);
          stream.isEOI = false;
        }
        else {
          stream.stream.push(text);
        }
        break;
      default:
        break;
      }
    }
  }
};

const processStaticRequest = (req, res, url) => {
  let relPath  = url.pathname;
  let rootPath = path.resolve(config.httpRoot);
  let absPath  = path.resolve(`${config.httpRoot}${relPath}`);
  if (os.type().startsWith("Windows")) {
    rootPath = rootPath.replaceAll("\\", "/");
    absPath  = absPath.replaceAll("\\", "/");
  }
  if (absPath === rootPath) {
    absPath += "/index.html";
  }
  if (absPath.startsWith(rootPath + "/")) {
    fs.access(absPath, fs.constants.F_OK, err => {
      if (err) {
        if (absPath === `${rootPath}/index.html`) {
          generateDefaultResponse(req, res);
        }
        else {
          res.writeHead(404);
          res.end();
          logHttpRequest(req, 404);
        }
      }
      else {
        fs.readFile(absPath, (err, data) => {
          if (err) {
            res.writeHead(500);
            res.end();
            logHttpRequest(req, 500);
          }
          else {
            let pi = absPath.lastIndexOf(".");
            let mediaType = (pi >= 0) ? mediaTypeMap[absPath.substr(pi + 1)] : "application/octet-stream";
            if (typeof mediaType === "undefined") mediaType = "application/octet-stream";
            res.writeHead(200, {"Content-Type":mediaType});
            res.write(data);
            res.end();
            logHttpRequest(req, 200);
          }
        });
      }
    });
  }
  else {
    res.writeHead(404);
    res.end();
    logHttpRequest(req, 404);
  }
};

const readConfigFile = path => {
  if (os.type().startsWith("Windows")) path = path.replaceAll("\\", "/");
  let si = path.lastIndexOf("/");
  const baseDir = (si >= 0) ? path.substring(0, si + 1) : "";
  let configObj = JSON.parse(fs.readFileSync(path));
  for (const key of Object.keys(configObj)) {
    let value = configObj[key];
    if (key === "machines") {
      for (const machine of value) {
        machineMap[machine.id] = machine;
        for (const propName of Object.keys(machine)) {
          let prop = machine[propName];
          if (typeof prop === "string" && prop.startsWith("%")) {
            const tokens = prop.substring(1).split("|");
            let path = tokens[0].startsWith("/") ? tokens[0] : `${baseDir}${tokens[0]}`;
            machine[propName] = readConfigProperty(path, tokens[1], tokens[2], tokens[3]);
          }
        }
        if (typeof machine.maxConnections === "undefined") machine.maxConnections = 1;
        machine.curConnections = 0;
      }
    }
    else if (typeof value === "string" && value.startsWith("%")) {
      const tokens = value.substring(1).split("|");
      let path = tokens[0].startsWith("/") ? tokens[0] : `${baseDir}${tokens[0]}`;
      config[key] = readConfigProperty(path, tokens[1], tokens[2], tokens[3]);
    }
    else {
      config[key] = value;
    }
  }
};

const readConfigProperty = (path, sectionName, propertyName, defaultValue) => {
  if (path.endsWith(".ini")) {
    const val = readConfigProperty(path.substring(0, path.lastIndexOf(".")) + ".ovl", sectionName, propertyName, null);
    if (val !== null) return val;
  }
  if (!fs.existsSync(path)) return defaultValue;
  const lines = fs.readFileSync(path, "utf8").split("\n");
  let i = 0;
  const sectionStart = `[${sectionName}]`;
  while (i < lines.length) {
    if (lines[i++].trim() === sectionStart) break;
  }
  while (i < lines.length) {
    let line = lines[i++].trim();
    if (line.startsWith("[")) break;
    let ei = line.indexOf("=");
    if (ei === -1) continue;
    let key = line.substring(0, ei).trim();
    if (key === propertyName) return line.substring(ei + 1).trim();
  }
  return defaultValue;
};

const sendData = (connection, streamType, streamId, data) => {
  connection.ws.sendUTF(`${streamType} ${streamId} ${data.length} ${data}`);
};

const usage = exitCode => {
  process.stderr.write("usage: node rjews [-h][--help][-p path][-v][--version] [path ...]\n");
  process.stderr.write("  -h, --help     display this usage information\n");
  process.stderr.write("  -p             pathname of file in which to write process id\n");
  process.stderr.write("  -v, --version  display program version number\n");
  process.stderr.write("  path           pathname(s) of configuration file(s)\n");
  process.exit(exitCode);
};

let configFiles = [];
let pidFile = null;

let i = 2;
while (i < process.argv.length) {
  let arg = process.argv[i++];
  if (arg.charAt(0) === "-") {
    switch (arg) {
    case "-h":
    case "--help":
      usage(0);
      break;
    case "-p":
      if (i < process.argv.length && process.argv[i].charAt(0) !== "-" && pidFile === null) {
        pidFile = process.argv[i++];
      }
      else {
        usage(1);
      }
      break;
    case "-v":
    case "--version":
      process.stdout.write(`v${VERSION}\n`);
      process.exit(0);
      break;
    default:
      usage(1);
      break;
    }
  }
  else {
    configFiles.push(arg);
  }
}

if (configFiles.length < 1) configFiles.push("rjews.json");
if (pidFile === null) pidFile = "rjews.pid";

for (const path of configFiles) {
  readConfigFile(path);
}

process.title = "rjews";

fs.writeFileSync(pidFile, `${process.pid}\n`);
log(`PID is ${process.pid}`);
const host = typeof config.host !== "undefined" ? config.host : "0.0.0.0";
const port = typeof config.port !== "undefined" ? config.port : 8080;

const httpServer = http.createServer((req, res) => {
  if (req.method === "GET") {
    let u = url.parse(req.url, true);
    if (u.pathname === "/machines") {
      processMachinesRequest(req, res, u.query);
    }
    else if (/^\/machine\/[^\/]+/.test(u.pathname)) {
      processMachineRequest(req, res, u.pathname);
    }
    else {
      processStaticRequest(req, res, u);
    }
  }
  else {
    res.writeHead(501);
    res.end();
    logHttpRequest(req, 501);
  }
});

httpServer.listen({host:host, port:port});
log(`Listening on address ${host}:${port}`);
if (typeof config.httpRoot === "undefined") config.httpRoot = "www";
log(`HTTP root ${config.httpRoot}`);

log("Machines served:");
for (const key of Object.keys(machineMap)) {
  let machine = machineMap[key];
  let id = key;
  while (id.length < 8) id += " ";
  log(`  ${id} : ${machine.title ? machine.title : ""} (${machine.host}:${machine.port})`);
}

for (const key of Object.keys(machineMap)) {
  let machine = machineMap[key];
  if (typeof machine.protocol === "undefined") {
    machine.protocol = "hasp";
  }
  else {
    switch (machine.protocol) {
    case "hasp":
    case "mode4":
      break;
    default:
      log(`Unrecognized RJE protocol: ${machine.protocol} in ${key}`);
      process.exit(1);
    }
  }
}

const wsServer = new WebSocketServer({
  httpServer: httpServer,
  autoAcceptConnections: false
});

wsServer.on("request", req => {
  let url = req.resourceURL;
  if (url.pathname.startsWith("/connections/")) {
    let si = url.pathname.lastIndexOf("/");
    let id = parseInt(url.pathname.substr(si + 1));
    if (id < connections.length) {
      let connection = connections[id];
      if (typeof connection.ws === "undefined") {
        logWsRequest(`${req.socket.remoteAddress} open ${url.pathname} (${connection.machineName})`);
        let ws = req.accept(null, req.origin);
        connection.ws = ws;
        connection.remoteAddress = req.socket.remoteAddress;
        connection.lastAction = Date.now();
        connection.inputBuffer = "";
        if (typeof connection.streams === "object") {
          for (const key of Object.keys(connection.streams)) {
            let stream = connection.streams[key];
            if (stream.data.length > 0) {
              sendData(connection, stream.type, stream.id, stream.data);
              stream.data = "";
              if (stream.isEOI) {
                connection.ws.sendUTF(`${stream.type} ${stream.id} 0 `);
              }
            }
          }
        }
        ws.on("message", msg => {
          if (msg.type === "utf8") {
            processReceivedData(connection, msg.utf8Data);
          }
          else {
            // ignore binary data
            logWsRequest(`${req.remoteAddress} binary data discarded from /connections/${id}`);
          }
          connection.lastAction = Date.now();
        });
        ws.on("close", (reason, description) => {
          logWsRequest(`${req.remoteAddress} close /connections/${id} (${connection.machineName})`);
          connection.isConnected = false;
          connection.service.end();
          delete connection.ws;
          machineMap[connection.machineName].curConnections -= 1;
        });
      }
      else {
        log(`${req.remoteAddress} Connection in use /connections/${id}`);
        req.reject(409);
      }
    }
    else {
      log(`${req.remoteAddress} No such connection /connections/${id}`);
      req.reject(404);
    }
  }
  else {
    log(`${req.remoteAddress} Not found ${url.pathname}`);
    req.reject(404);
  }
});

wsServer.on("close", (ws, reason, description) => {
});

//
//  Periodically traverse the connection list to seek connections that
//  have been idle too long, and close them.
//
setInterval(() => {
  let currentTime = Date.now();
  for (let connection of connections) {
    if (connection.isConnected === true
        && connection.idleTimeout > 0
        && typeof connection.lastAction !== "undefined"
        && currentTime - connection.lastAction >= connection.idleTimeout) {
      logWsRequest(`${connection.remoteAddress} idle timeout /connections/${connection.id} (${connection.machineName})`);
      if (typeof connection.ws !== "undefined") {
        connection.ws.close();
        delete connection.ws;
      }
      if (typeof connection.service !== "undefined") connection.service.end();
      connection.isConnected = false;
    }
  }
}, 10000);
