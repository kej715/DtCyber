#!/usr/bin/env node
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2017, Kevin Jordan
**
**  webterm-server.js
**    This program implements a server for browser-based terminal emulators.
**    It enables browsers to load terminal emulators, and it enables the
**    terminal emulators to interact with host-based machine simulators
**    such as DtCyber.
**
**--------------------------------------------------------------------------
*/

const fs = require("fs");
const http = require("http");
const mime = require("mime");
const net = require("net");
const os = require('node:os');
const path = require("path");
const url = require("url");
const WebSocketServer = require("websocket").server;

const VERSION = "1.1";

let connections = [];
let config = {};
let machineMap = {};

const generateDefaultResponse = (req, res) => {
  res.writeHead(200, {"Content-Type":"text/html"});
  res.write("<html>");
  res.write(  "<head>");
  res.write(    '<link rel="stylesheet" href="css/styles.css" />');
  res.write(    '<link rel="icon" type="image/png" href="images/ncc.png">');
  res.write(    '<script src="js/jquery-3.3.1.min.js"></script>');
  res.write(  "</head>");
  res.write(  "<body>");
  res.write(    "<h1>Available Services</h1>");
  res.write(    '<ul id="machines" class="machine-list"></ul>');

  for (const key of Object.keys(machineMap)) {
    let machine = machineMap[key];
    res.write(`<li><a href="${machine.terminal}" target="_blank">${key}</a> : ${machine.title}</li>`);
  }

  res.write("</ul></body></html>");
  res.end();
  logHttpRequest(req, 200);
}

const getAvailableConnection = () => {

  for (let connection of connections) {
    if (connection.connected === false) return connection;
  }

  let newConnection = {
    id: connections.length,
    connected: false
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
      let server = {
        host: machine.host,
        port: machine.port
      };
      let client = new net.Socket();
      let connection = getAvailableConnection();
      connection.machineName = query.machine;
      connection.client = client;
      if (typeof machine.protocol !== "undefined" && machine.protocol.toLowerCase() === "telnet") {
        connection.isTelnetProtocol = true;
      }
      else {
        connection.isTelnetProtocol = false;
      }
      connection.crlf = machine.crlf;
      connection.command = [];
      connection.data = Buffer.allocUnsafe(0);
      delete connection.ws;

      client.on("error", err => {
        if (connection.ws) {
          connection.ws.close();
          delete connection.ws;
          connection.connected = false;
        }
        else {
          res.writeHead(500);
          res.end(err.message ? err.message : `Failed to connect to ${machine}`);
          logHttpRequest(req, 500);
        }
      });

      client.connect(server, () => {
        connection.connected = true;
        connection.echo = false;
        res.writeHead(200, {"Content-Type":"text/plain"});
        res.end(connection.id.toString());
        logHttpRequest(req, 200);
        connection.client.on("data", data => {
          if (connection.isTelnetProtocol) {
            // Telnet protocol elements
            // 255 - IAC
            // 254 - DON'T
            // 253 - DO
            // 252 - WON'T
            // 251 - WILL
            // 250 - Subnegotiation Begin
            // 259 - Go Ahead
            // 248 - Erase Line
            // 247 - Erase Character
            // 246 - AYT
            // 245 - Abort Output
            // 244 - Interrupt Process
            // 243 - Break
            // 242 - Data Mark
            // 241 - No Op
            // 240 - Subnegotiation End
            //
            // Telnet options
            //  0 - Binary Mode
            //  1 - Echo
            //  3 - Suppress Go Ahead
            // 24 - Terminal Type
            // 34 - Line Mode

            let i = 0;
            let start = 0;
            while (i < data.length) {
              if (connection.command.length > 0) {
                connection.command.push(data[i]);
                if (connection.command.length > 2) {
                  switch (connection.command[1]) {
                  case 254: // DON'T
                    connection.command[1] = 252; // WON'T
                    if (connection.command[2] === 1) { // Echo
                      connection.echo = false;
                    }
                    connection.client.write(Buffer.from(connection.command));
                    connection.command = [];
                    break;
                  case 253: // DO
                    switch (connection.command[2]) {
                    case 0:  // Binary
                    case 3:  // SGA
                    case 24: // Terminal Type
                      connection.command[1] = 251; // WILL
                      break;
                    case 1:  // Echo
                      connection.echo = true;
                      connection.command[1] = 251; // WILL
                      break;
                    default:
                      connection.command[1] = 252; // WON'T
                      break;
                    }
                    connection.client.write(Buffer.from(connection.command));
                    connection.command = [];
                    break;
                  case 252: // WON'T
                    connection.command[1] = 254; // DON'T
                    connection.client.write(Buffer.from(connection.command));
                    connection.command = [];
                    break;
                  case 251: // WILL
                    if (connection.command[2] === 0       // Binary
                        || connection.command[2] === 1    // Echo
                        || connection.command[2] === 3) { // SGA
                      connection.command[1] = 253; // DO
                    }
                    else {
                      connection.command[1] = 254; // DON'T
                    }
                    connection.client.write(Buffer.from(connection.command));
                    connection.command = [];
                    break;
                  case 250: // SB
                    if (connection.command[connection.command.length - 1] === 240) { // if SE
                      if (connection.command.length > 3
                          && connection.command[2] === 24    // Terminal Type
                          && connection.command[3] === 1) {  // Send
                        connection.command[3] = 0; // Is
                        connection.client.write(Buffer.from(connection.command.slice(0, 4)));
                        connection.client.write("XTERM");
                        connection.command[1] = 240; // SE
                        connection.client.write(Buffer.from(connection.command.slice(0, 2)));
                      }
                      connection.command = [];
                    }
                    break;
                  default:
                    log(`Unsupported Telnet protocol option: ${connection.command[1]}`);
                    connection.command = [];
                    break;
                  }
                  start = i + 1;
                }
                else if (connection.command.length > 1) {
                  switch (connection.command[1]) {
                  case 241: // NOP
                  case 242: // Data Mark
                  case 243: // Break
                  case 244: // Interrupt Process
                  case 245: // Abort Output
                  case 246: // Are You There
                  case 247: // Erase Character
                  case 248: // Erase Line
                  case 249: // Go Ahead
                    connection.command = [];
                    start = i + 1;
                  case 250: // SB
                  case 251: // WILL
                  case 252: // WON'T
                  case 253: // DO
                  case 254: // DON'T
                    // just collect the byte
                    break;
                  case 255: // escaped FF
                    if (connection.ws) {
                      connection.ws.sendBytes(Buffer.from([255]));
                      connection.lastAction = Date.now();
                    }
                    else {
                      connection.data = Buffer.concat([connection.data, [255]], connection.data.length + 1);
                    }
                    connection.command = [];
                    start = i + 1;
                    break;
                  default:
                    log(`Unsupported Telnet protocol option: ${connection.command[1]}`);
                    connection.command = [];
                    break;
                  }
                }
              }
              else if (data[i] === 255) { // IAC
                connection.command.push(data[i]);
                if (i > start) {
                  if (connection.ws) {
                    connection.ws.sendBytes(data.slice(start, i));
                    connection.lastAction = Date.now();
                  }
                  else {
                    connection.data = Buffer.concat([connection.data, data.slice(start, i)]);
                  }
                }
              }
              else if (data[i] === 0x0d && i + 1 < data.length && data[i + 1] === 0x00) {
                if (connection.ws) {
                  connection.ws.sendBytes(data.slice(start, i + 1));
                  connection.lastAction = Date.now();
                }
                else {
                  connection.data = Buffer.concat([connection.data, data.slice(start, i + 1)]);
                }
                start = i + 2;
                i += 1;
              }
              i += 1;
            }
            if (i > start && connection.command.length < 1) {
              if (connection.ws) {
                connection.ws.sendBytes(data.slice(start, i));
                connection.lastAction = Date.now();
              }
              else {
                connection.data = Buffer.concat([connection.data, data.slice(start, i)],
                  connection.data.length + (i - start));
              }
            }
          }
          //
          // Raw, non-telnet connection
          //
          else if (connection.ws) {
            connection.ws.sendBytes(data);
            connection.lastAction = Date.now();
          }
          else {
            connection.data = Buffer.concat([connection.data, data], connection.data.length + data.length);
          }
        });

        connection.client.on("close", data => {
          if (connection.ws) {
            connection.ws.close();
            delete connection.ws;
            connection.connected = false;
          }
        });
      });
    }
    else {
      res.writeHead(404);
      res.end();
      logHttpRequest(req, 404);
    }
  }
  else {
    res.writeHead(400);
    res.end();
    logHttpRequest(req, 400);
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
            let mediaType = (pi >= 0) ? mime.getType(absPath.substr(pi + 1)) : "application/octet-stream";
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
  const configObj = readConfigObject(path);
  for (const key of Object.keys(configObj)) {
    if (key === "machines") {
      for (const machine of configObj[key]) {
        machineMap[machine.id] = machine;
      }
    }
    else {
      config[key] = configObj[key];
    }
  }
  //
  // Traverse the machine map and remove definitions for
  // examples that have unsatisfied dependencies upon the
  // machines installed products.
  //
  for (const key of Object.keys(machineMap)) {
    let machine = machineMap[key];
    if (typeof machine.examples !== "undefined"
        && Array.isArray(machine.examples)
        && typeof machine.installed !== "undefined"
        && Array.isArray(machine.installed)) {
      let i = 0;
      while (i < machine.examples.length) {
        let example = machine.examples[i];
        if (typeof example.depends === "string"
            && machine.installed.indexOf(example.depends) === -1) {
          machine.examples.splice(i, 1);
        }
        else {
          i += 1;
        }
      }
    }
  }
};

const readConfigObject = path => {
    if (os.type().startsWith("Windows")) path = path.replaceAll("\\", "/");
  let i = path.lastIndexOf("/");
  const baseDir = (i >= 0) ? path.substring(0, path.lastIndexOf("/") + 1) : "";
  log(`read configuration file ${path}`);
  let configObj = JSON.parse(fs.readFileSync(path));
  for (const key of Object.keys(configObj)) {
    if (key === "machines") {
      let machines = configObj[key];
      for (const machine of machines) {
        if (typeof machine === "string") { // relative pathname of machine definition
          machine = readConfigObject(machine.startsWith("/") ? machine : `${baseDir}${machine}`);
        }
        for (const key of Object.keys(machine)) {
          let val = machine[key];
          if (typeof val === "string" && val.startsWith("@")) {
            val = val.substring(1);
            machine[key] = readConfigObject(val.startsWith("/") ? val : `${baseDir}${val}`);
          }
        }
      }
    }
    else if (key === "httpRoot") {
      let httpRoot = configObj[key];
      if (os.type().startsWith("Windows")) httpRoot = httpRoot.replaceAll("\\", "/");
      configObj[key] = httpRoot.startsWith("/") ? httpRoot : `${baseDir}${httpRoot}`;
    }
  }
  return configObj;
};

const usage = exitCode => {
  process.stderr.write("usage: node webterm-server [-h][--help][-p path][-v][--version] [path ...]\n");
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

if (configFiles.length < 1) configFiles.push("webterm.json");
if (pidFile === null) pidFile = "webterm.pid";

for (const path of configFiles) {
  readConfigFile(path);
}

process.title = "webterm-server";

fs.writeFileSync(pidFile, `${process.pid}\n`);
log(`PID is ${process.pid}`);

log("Machines served:");
for (const key of Object.keys(machineMap)) {
  let machine = machineMap[key];
  let id = key;
  while (id.length < 8) id += " ";
  log(`  ${id} : ${machine.title ? machine.title : ""}`);
}

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

let port = config.port ? config.port : 8080;
httpServer.listen(port);
log(`Listening on port ${port}`);
if (typeof config.httpRoot === "undefined") config.httpRoot = "www";
log(`HTTP root ${config.httpRoot}`);

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
        if (connection.data.length > 0) {
          ws.sendBytes(connection.data);
          connection.data = Buffer.allocUnsafe(0);
        }
        ws.on("message", msg => {
          if (msg.type === "utf8") {
            let s = msg.utf8Data;
            if (connection.crlf) {
              s = s.split("\r").join("\r\n");
            }
            if (connection.isTelnetProtocol && s.indexOf("\xff") >= 0) {
              s = s.split("\xff").join("\xff\xff");
            }
            connection.client.write(s, "utf8");
            if (connection.echo) ws.sendUTF(s);
          }
          else {
            let binaryData = msg.binaryData;
            if (connection.isTelnetProtocol) {
              const ary = new Uint8Array(binaryData);
              let bytes = [];
              let start = 0;
              while (start < ary.length) {
                let i = ary.indexOf(0xff, start);
                if (i >= 0) {
                  bytes = bytes.concat(ary.slice(start, i + 1), 0xff);
                  start = i + 1;
                }
                else if (start === 0) {
                  break;
                }
                else {
                  bytes = bytes.concat(ary.slice(start));
                  break;
                }
              }
              if (bytes.length > 0) binaryData = new Uint8Array(bytes);
            }
            connection.client.write(binaryData);
            if (connection.echo) ws.sendBytes(msg.binaryData);
          }
          connection.lastAction = Date.now();
        });
        ws.on("close", (reason, description) => {
          logWsRequest(`${req.remoteAddress} close /connections/${id} (${connection.machineName})`);
          connection.connected = false;
          connection.client.end();
          delete connection.ws;
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
    if (connection.connected === true
        && typeof connection.lastAction !== "undefined"
        && currentTime - connection.lastAction >= 30 * 60 * 1000) {
      logWsRequest(`${connection.remoteAddress} idle timeout /connections/${connection.id} (${connection.machineName})`);
      if (typeof connection.ws !== "undefined") {
        connection.ws.close();
        delete connection.ws;
      }
      if (typeof connection.client !== "undefined") connection.client.end();
      connection.connected = false;
    }
  }
}, 10000);
