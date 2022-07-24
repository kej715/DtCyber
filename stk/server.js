const fs = require("fs");
const Global = require("./global");
const http = require("http");
const process = require("process");
const ProgramRegistry = require("./registry");
const PortMapper = require("./portmapper");
const StkCSI = require("./stkcsi");
const url = require("url");

process.title = "STK4400";

fs.writeFileSync("pid", `${process.pid}\n`);
console.log(`${new Date().toLocaleString()} PID is ${process.pid}`);

const configFile = (process.argv.length > 2) ? process.argv[2] : "config.json";
const config = JSON.parse(fs.readFileSync(configFile));

const volumesFile = (process.argv.length > 3) ? process.argv[3] : "volumes.json";
let volumeMap = JSON.parse(fs.readFileSync(volumesFile));

Global.programRegistry = new ProgramRegistry();
if (config.debug) Global.programRegistry.setDebug(config.debug);

if (config.foreignPortMapper) {
  Global.programRegistry.setForeignPortMapper(config.foreignPortMapper);
  let pm = Global.programRegistry.getForeignPortMapper();
  console.log(`${new Date().toLocaleString()} Use foreign portmapper at UDP address ${pm.host}:${pm.port}`);
}
else {
  const portMapper = new PortMapper();
  if (config.portMapperUdpPort) portMapper.setUdpPort(config.portMapperUdpPort);
  if (config.debug) portMapper.setDebug(config.debug);
  console.log(`${new Date().toLocaleString()} Start built-in portmapper on UDP port ${portMapper.udpPort}`);
  portMapper.start();
}

const stkcsi = new StkCSI();
stkcsi.setVolumeMap(volumeMap);
if (config.tapeServerPort)  stkcsi.setTapeServerPort(config.tapeServerPort);
if (config.tapeRobotPort)   stkcsi.setTapeRobotPort(config.tapeRobotPort);
if (config.tapeCacheRoot)   stkcsi.setTapeCacheRoot(config.tapeCacheRoot);
if (config.tapeLibraryRoot) stkcsi.setTapeLibraryRoot(config.tapeLibraryRoot);
if (config.debug)           stkcsi.setDebug(config.debug);
stkcsi.start();

const logHttpRequest = (req, status) => {
  console.log(`${new Date().toLocaleString()} HTTP ${req.socket.remoteAddress} ${status} ${req.method} ${req.url}`);
};

const httpServer = http.createServer((req, res) => {
  const corsHeaders = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "OPTIONS, GET",
    "Access-Control-Allow-Headers": "X-Requested-With",
    "Access-Control-Max-Age": 2592000, // 30 days
  };
  if (req.method === "GET") {
    let u = url.parse(req.url, true);
    if (u.pathname === "/volumes") {
      if (u.search === "?tfsp") {
        let headers = {"Content-Type":"text/plain"};
        Object.keys(corsHeaders).forEach(key => {
          headers[key] = corsHeaders[key];
        });
        let body = "";

        Object.keys(stkcsi.volumeMap).forEach(key => {
          let volume = stkcsi.volumeMap[key];
          if (typeof volume.tfspManaged === "undefined" || volume.tfspManaged) {
            body += `VSN=${key}`;
            if (typeof volume.physicalName !== "undefined") body += `,PRN=${volume.physicalName}`;
            if (typeof volume.userOwned !== "undefined" && volume.userOwned) {
              body += ",OWNER=USER,SYSTEM=NO";
            }
            else {
              body += `,OWNER=CENTER,SYSTEM=${(typeof volume.systemVsn !== "undefined" && volume.systemVsn) ? "YES" : "NO"}`;
            }
            body += ",SITE=ON,VT=AT,GO.\n";
            if (typeof volume.owner !== "undefined") {
              body += `USER=${volume.owner}\n`;
              body += `FILEV=${key}`;
              body += `,AC=${(typeof volume.listable !== "undefined" && volume.listable) ? "YES" : "NO"}`;
              body += `,CT=${(typeof volume.access !== "undefined") ? volume.access : "PRIVATE"}`;
              body += ",D=AE";
              body += `,F=${(typeof volume.format !== "undefined") ? volume.format : "I"}`;
              body += `,LB=${(typeof volume.labeled !== "undefined" && volume.labeled) ? "KL" : "KU"}`;
              body += `,M=${(typeof volume.readOnly !== "undefined" && !volume.readOnly) ? "WRITE" : "READ"}`;
              body += ",GO\n";
              body += "GO\n";
            }
          }
        });

        res.writeHead(200, headers);
        res.write(body);
      }
      else {
        let headers = {"Content-Type":"application/json"};
        Object.keys(corsHeaders).forEach(key => {
          headers[key] = corsHeaders[key];
        });
        res.writeHead(200, headers);
        res.write(JSON.stringify(stkcsi.volumeMap));
      }
      res.end();
      logHttpRequest(req, 200);
    }
    else {
      res.writeHead(404);
      res.end();
      logHttpRequest(req, 404);
    }
  }
  else if (req.method === "OPTIONS") {
    res.writeHead(204, corsHeaders);
    res.end();
    logHttpRequest(req, 204);
  }
  else {
    res.writeHead(501);
    res.end();
    logHttpRequest(req, 501);
  }
});

const httpServerPort = config.httpServerPort ? config.httpServerPort : 4480;
httpServer.listen(httpServerPort);
console.log(`${new Date().toLocaleString()} HTTP server listening on port ${httpServerPort}`);

fs.watch(volumesFile, (eventType, filename) => {
  if (eventType === "change") {
    try {
      let volumeMap = JSON.parse(fs.readFileSync(filename));
      Object.keys(volumeMap).forEach(key => {
        if (typeof stkcsi.volumeMap[key] !== "undefined") {
          let volume = volumeMap[key];
          let updatedAttrs = [];
          Object.keys(volume).forEach(attrName => {
            if (stkcsi.volumeMap[key][attrName] !== volume[attrName]) {
              updatedAttrs.push(attrName);
            }
            stkcsi.volumeMap[key][attrName] = volume[attrName];
          });
          if (updatedAttrs.length > 0) {
            console.log(`${new Date().toLocaleString()} Updated ${updatedAttrs.join(",")} of VSN ${key} in tape library`);
          }
        }
        else {
          stkcsi.volumeMap[key] = volumeMap[key];
          console.log(`${new Date().toLocaleString()} Added VSN ${key} to tape library`);
        }
      });
    }
    catch (err) {
      console.log(`${new Date().toLocaleString()} Failed to update volume map`);
      console.log(`${new Date().toLocaleString()} ${err}`);
    }
  }
});
