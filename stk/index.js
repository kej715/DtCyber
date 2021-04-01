const fs = require("fs");
const PortMapper = require("./portmapper");
const StkCSI = require("./stkcsi");

const configFile = (process.argv.length > 2) ? process.argv[2] : "config.json";
const config = JSON.parse(fs.readFileSync(configFile));

const volumesFile = (process.argv.length > 3) ? process.argv[3] : "volumes.json";
let volumeMap = JSON.parse(fs.readFileSync(volumesFile));

const portMapper = new PortMapper();
if (config.portMapperUdpPort) portMapper.setUdpPort(config.portMapperUdpPort);
if (config.debug) portMapper.setDebug(config.debug);
portMapper.start();

const stkcsi = new StkCSI();
stkcsi.setVolumeMap(volumeMap);
if (config.tapeServerPort) stkcsi.setTapeServerPort(config.tapeServerPort);
if (config.tapeLibraryRoot) stkcsi.setTapeLibraryRoot(config.tapeLibraryRoot);
if (config.debug) stkcsi.setDebug(config.debug);
stkcsi.start();

fs.watch(volumesFile, (eventType, filename) => {
  if (eventType === "change") {
    volumeMap = JSON.parse(fs.readFileSync(filename));
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
          console.log(`${new Date().toLocaleString()} updated ${updatedAttrs.join(",")} of VSN ${key} in library`);
        }
      }
      else {
        stkcsi.volumeMap[key] = volumeMap[key];
        console.log(`${new Date().toLocaleString()} added VSN ${key} to library`);
      }
    });
  }
});
