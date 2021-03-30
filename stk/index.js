const fs = require("fs");
const PortMapper = require("./portmapper");
const StkCSI = require("./stkcsi");

const configFile = (process.argv.length > 2) ? process.argv[2] : "config.json";
const config = JSON.parse(fs.readFileSync(configFile));

const volumesFile = (process.argv.length > 3) ? process.argv[3] : "volumes.json";
let volumeMap = JSON.parse(fs.readFileSync(volumesFile));

const portMapper = new PortMapper();
if (config.portMapperUdpPort) portMapper.setUdpPort(config.portMapperUdpPort);
portMapper.start();
const stkcsi = new StkCSI();
stkcsi.setVolumeMap(volumeMap);
if (config.tapeServerPort) stkcsi.setTapeServerPort(config.tapeServerPort);
stkcsi.start();
fs.watch(volumesFile, (eventType, filename) => {
  if (eventType === "change") {
    console.log(`${new Date().toLocaleString()} reread ${filename}`);
    volumeMap = JSON.parse(fs.readFileSync(filename));
    stkcsi.setVolumeMap(volumeMap);
  }
});
