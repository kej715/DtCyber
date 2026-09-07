#!/usr/bin/env node

const DtCyber   = require("../automation/DtCyber");
const fs        = require("fs");
const Terminal  = require("../automation/Terminal");
const utilities = require("./opt/utilities");

const dtc         = new DtCyber();
const term        = new Terminal.AnsiTerminal();
const customProps = utilities.getCustomProperties(dtc);

let   completedSteps     = [];
const completedStepsPath = "nve-install-steps";
const dsTapeLabel        = "VE857";

const media = {
  "5V001A": {name: "nve857-5v001a.tap",     url: "https://www.dropbox.com/scl/fi/8spr5ct4xujh7f82okqpq/nve857-5v001a.tap?rlkey=p7ojd0dibn8npx0nctqtcj6th&dl=1"},
  "5V001B": {name: "nve857-5v001b.tap",     url: "https://www.dropbox.com/scl/fi/jviu76jag0nqdaniik28i/nve857-5v001b.tap?rlkey=jy46czir6kig1oxi0tb0gdtfr&dl=1"},
  "5V001C": {name: "nve857-5v001c.tap",     url: "https://www.dropbox.com/scl/fi/cvfinsyu3l27klj4g95od/nve857-5v001c.tap?rlkey=gmof9wufi8iu8ajz3u7y4fzou&dl=1"},
  "5V001D": {name: "nve857-5v001d.tap",     url: "https://www.dropbox.com/scl/fi/shtxl0hyzzz3kf4shonuk/nve857-5v001d.tap?rlkey=sp5s67dcy3nk665d8u2kyztsb&dl=1"},
  "5V001E": {name: "nve857-5v001e.tap",     url: "https://www.dropbox.com/scl/fi/2b5uc9z6q1779hsdovn3d/nve857-5v001e.tap?rlkey=50spfww1p62jfzbhqsob6webn&dl=1"},
  "5V001F": {name: "nve857-5v001f.tap",     url: "https://www.dropbox.com/scl/fi/icnbfc687ltpba5o7fa56/nve857-5v001f.tap?rlkey=xanx30ac40mah4rqqhvqxebf2&dl=1"},
  "AS002V": {name: "nve857-bcu-as002v.tap", url: "https://www.dropbox.com/scl/fi/zosfdjdkqeknzfo92or67/nve857-bcu-as002v.tap?rlkey=6wsdgnh58mr37u60fypzktt8k&dl=1"},
  "1J006A": {name: "NVEL826AOF7.tap",       url: "https://www.dropbox.com/scl/fi/1wy58xm060kxhrlwc2ptm/NVEL826AOF7.tap?rlkey=y8xxabiyv8hz0x6r352gbr6me&dl=1"},
  "1J006B": {name: "NVEL826BOF7.tap",       url: "https://www.dropbox.com/scl/fi/hrfse9a48p9t1vev0phgn/NVEL826BOF7.tap?rlkey=pb613wi7sr19bghlwskt4tj1a&dl=1"},
  "1J006C": {name: "NVEL826COF7.tap",       url: "https://www.dropbox.com/scl/fi/ry7tb0yvt2l4hkhy5rigp/NVEL826COF7.tap?rlkey=bgn273yj4phwbmmwk8p5w718s&dl=1"},
  "1J006D": {name: "NVEL826DOF7.tap",       url: "https://www.dropbox.com/scl/fi/mo4kxq8rabozq81of31vi/NVEL826DOF7.tap?rlkey=9l84z1jn3uqmg0iwtvq3i7du3&dl=1"},
  "1J006E": {name: "NVEL826EOF7.tap",       url: "https://www.dropbox.com/scl/fi/r6qfzqrql56ywg9orjlwf/NVEL826EOF7.tap?rlkey=cb83cdi4wtffza8j6b6g999ab&dl=1"},
  "1J006F": {name: "NVEL826FOF7.tap",       url: "https://www.dropbox.com/scl/fi/usuuttk97q0ujxemix9pg/NVEL826FOF7.tap?rlkey=in3u94v25arhj29uky2a9unju&dl=1"},
  "CG019A": {name: "L847_CG019A.tap",       url: "https://www.dropbox.com/scl/fi/muwqbnxi65edns1soggbw/L847_CG019A.tap?rlkey=nd7mi09yh950yc6d21m0rjwf0&st=u8q986o6&dl=1"},
  "CG019B": {name: "L847_CG019B.tap",       url: "https://www.dropbox.com/scl/fi/31as0l57nu6y0cme97xuc/L847_CG019B.tap?rlkey=k2igo7632ll3amknouvuxquer&st=xhqyfacw&dl=1"},
  "CG019C": {name: "L847_CG019C.tap",       url: "https://www.dropbox.com/scl/fi/javrevwnsz2k1xfb6k171/L847_CG019C.tap?rlkey=s83iluzuh6lh2xcnz8anzy3gr&st=qirpr80x&dl=1"},
  "CG019D": {name: "L847_CG019D.tap",       url: "https://www.dropbox.com/scl/fi/wgsi5hzy70flhw8qqx79u/L847_CG019D.tap?rlkey=c3b3xud9x43nyxab1oum5tfpe&st=57313lcg&dl=1"}
};

const coreCommands = [
  "setsa network_activation 0",
  "setsa unload_deadstart_tape 0",
  "setit packing_list_857 '5V001A',,mt9$6250",
  "inisd NVE01",
  "go"
];

const dcfile = [
  "DCF01",
  "setsa network_activation 0",
  "setsa unload_deadstart_tape 0"
];

const lcuCommands = [
  "inimv disk2 nve02",
  "addvts nve02",
  "inimv disk3 nve03",
  "addvts nve03",
  "inimv disk4 nve04",
  "addvts nve04",
  "quit"
];

const pcuCommands = [
  "deled $deadstart_device",
  "deled $deadstart_controller",
  "deled $system_device",
  "deled $system_controller",
  "added ctlr1 ei=$7155_12 sn=1 ic=((ch16))",
  "added ctlr2 ei=$7155_12 sn=2 ic=((ch18))",
  "added ctlr3 ei=$7021_32 sn=1 ic=((ch17))",
  "added disk1 ei=$885_12 sn=1 pc=((ctlr1,32))",
  "added disk2 ei=$885_12 sn=2 pc=((ctlr2,33))",
  "added disk3 ei=$885_12 sn=3 pc=((ctlr1,34))",
  "added disk4 ei=$885_12 sn=4 pc=((ctlr2,35))",
  "added tape1 ei=$679_7 sn=1 pc=((ctlr3,0))",
  "added tape2 ei=$679_7 sn=2 pc=((ctlr3,1))",
  "quit"
];

const productList = [
  {name: "AFTERBURNER",                packingList: "packing_list_857"},
  {name: "BASIC",                      packingList: "packing_list_826"},
  {name: "BUILD_UTILITY",              packingList: "packing_list_857"},
  {name: "CML",                        packingList: "packing_list_857"},
  {name: "COBOL",                      packingList: "packing_list_857"},
  {name: "CYBIL",                      packingList: "packing_list_847"},
  {name: "DEBUG",                      packingList: "packing_list_857"},
  {name: "DESKTOP_VE",                 packingList: "packing_list_857"},
  {name: "DVS",                        packingList: "packing_list_857"},
  {name: "EDIT_CATALOG",               packingList: "packing_list_857"},
  {name: "FILE_MANAGER",               packingList: "packing_list_857"},
  {name: "FMA",                        packingList: "packing_list_857"},
  {name: "FMU",                        packingList: "packing_list_857"},
  {name: "FORMAT_CYBIL_SOURCE",        packingList: "packing_list_857"},
  {name: "FORTRAN_V2_RUNTIME_LIBRARY", packingList: "packing_list_857"},
  {name: "FORTRAN_VERSION_1",          packingList: "packing_list_857"},
  {name: "FORTRAN_VERSION_2",          packingList: "packing_list_857"},
  {name: "KERMIT",                     packingList: "packing_list_857"},
  {name: "LINE_PRINTER_MANUALS",       packingList: "packing_list_857"},
  {name: "MAILVE_VERSION_2",           packingList: "packing_list_857"},
  {name: "MALET",                      packingList: "packing_list_857"},
  {name: "MENU_VE",                    packingList: "packing_list_857"},
  {name: "NOSVE_FILES",                packingList: "packing_list_857"},
  {name: "NOSVE_MAINTENANCE",          packingList: "packing_list_857"},
  {name: "ONLINE_MANUALS",             packingList: "packing_list_857"},
  {name: "ONLINE_MANUALS_SOURCE",      packingList: "packing_list_857"},
  {name: "PASCAL",                     packingList: "packing_list_857"},
  {name: "PFTF",                       packingList: "packing_list_857"},
  {name: "PPE",                        packingList: "packing_list_857"},
  {name: "PPM",                        packingList: "packing_list_857"},
  {name: "PROGRAMMING_ENVIRONMENT",    packingList: "packing_list_857"},
  {name: "SDF",                        packingList: "packing_list_857"},
  {name: "SVS",                        packingList: "packing_list_857"},
  {name: "XMODEM",                     packingList: "packing_list_857"}
];

class VikingFilter {

  constructor(emulator) {
    //
    // Character reception states
    //
    this.ST_BASE     = 0;
    this.ST_RS_1     = 1;
    this.ST_W_CURSOR = 2;
    this.ST_DEF_FN   = 3;
    this.ST_REPORT   = 4;
    this.ST_LOAD     = 5;
    this.ST_X_CHAR   = 6;
    this.ST_SET_SF   = 7;
    this.ST_RS_DC2   = 8;

    this.emulator         = emulator;
    this.height           = 30;
    this.scrollFieldLower = this.height - 1;
    this.scrollFieldUpper = 0;
    this.state            = this.ST_BASE;
    this.width            = 80;
    this.wrap             = true;
    this.x                = 0;
    this.y                = 0;
    this.screen           = new Uint8Array(this.width * this.height);
    for (let i = 0; i < this.screen.length; i++) this.screen[i] = 0;
  }

  clearScreen() {
    this.x      = 0;
    this.y      = 0;
    for (let i = 0; i < this.screen.length; i++) this.screen[i] = 0;
  }

  get(x, y) {
    if (x < this.width && y < this.height) {
      return this.screen[(y * this.width) + x];
    }
    else {
      return 0;
    }
  }

  getString(x, y, len) {
    let s = "";
    for (let i = 0; i < len; i++) {
      s += String.fromCharCode(this.get(x, y));
      x += 1;
    }
    return s;
  }

  processBaseChar(b) {
    if (b >= 0x20 && b < 0x7f) {
      this.put(this.x, this.y, b);
      this.x += 1;
      this.emulator.renderText(String.fromCharCode(b));
    }
    else {
      switch (b) {
      default:
        break;
      case 0x02: // STX - write cursor address
        this.params = [];
        this.state = this.ST_W_CURSOR;
        break;
      case 0x03: // ETX - enable blink
        break;
      case 0x04: // EOT - disable blink
        break;
      case 0x05: // ENQ - read cursor address
        break;
      case 0x06: // ACK - start underline
        break;
      case 0x07: // BEL
        break;
      case 0x08: // BS  - cursor left
      case 0x1F: // US  - cursor left
        if (this.x > 0) this.x -= 1;
        this.emulator.renderText("\b");
        break;
      case 0x09: // HT  - tab
        break;
      case 0x0a: // LF  - cursor down
      case 0x1a: // SUB - cursor down
        if (this.y < (this.height - 1)) {
          this.y += 1;
        }
        else {
          this.y = 0;
        }
        this.emulator.renderText("\n");
        break;
      case 0x0b: // VT  - erase to end of line
        break;
      case 0x0c: // FF  - erase page
        break;
      case 0x0d: // CR  - carriage return
        this.x = 0;
        break;
      case 0x0e: // SO  - start blink
        break;
      case 0x0f: // SI  - stop blink
        break;
      case 0x11: // DC1 - x-on
        break;
      case 0x12: // DC2 - roll enable
        break;
      case 0x13: // DC3 - x-off
        break;
      case 0x15: // NAK - end underscore
        break;
      case 0x16: // SYN - roll disable
        break;
      case 0x17: // ETB - cursor up
        if (this.y > 0) this.y -= 1;
        break;
      case 0x18: // CAN - skip
        break;
      case 0x19: // EM  - home
        this.x = 0;
        this.y = 0;
        break;
      case 0x1c: // FS  - start dim
        break;
      case 0x1d: // GS  - end dim
        break;
      case 0x1e: // RS  - enter RS1 state
        this.state = this.ST_RS_1;
        break;
      }
    }
  }

  processDC2(b) {
    switch (b) {
    default:
      break;
    case 0x42: // enter large CYBER mode
      break;
    case 0x48: // set 80 character line
      this.width  = 80;
      this.screen = new Uint8Array(this.width * this.height);
      this.clearScreen();
      break;
    case 0x5e: // select 30 lines
      this.height = 30;
      this.screen = new Uint8Array(this.width * this.height);
      this.clearScreen();
      break;
    }
    this.state = this.ST_BASE;
  }

  processDefFn(b) {
  }

  processExtChar(b) {
  }

  processKeyboardEvent(key, isShift, isCtrl, isAlt) {
    this.emulator.processKeyboardEvent(key, isShift, isCtrl, isAlt);
  }

  processLoadRAM(b) {
  }

  processReport(b) {
  }

  processRS1(b) {
    switch (b) {
    default:
      break;
    case 0x12: // RS DC2
      this.state = this.ST_RS_DC2;
      return;
    case 0x2a: // clear to end of line
      for (let y = this.y; y < this.width; y++) this.put(this.x, y, 0);
      break;
    case 0x33: // disable host loaded code
      break;
    case 0x44: // start inverse
      break;
    case 0x45: // end inverse
      break;
    case 0x55: // field scroll up
      for (let y = this.scrollFieldUpper + 1; y <= this.scrollFieldLower; y++) {
        for (let x = 0; x < this.width; x++) {
          this.put(x, y - 1, this.get(x, y));
        }
      }
      for (let x = 0; x < this.width; x++) this.put(x, this.scrollFieldLower, 0);
      this.emulator.renderText("\n");
      break;
    case 0x57: // set scroll field
      this.params = [];
      this.state  = this.ST_SET_SF;
      return;
    }
    this.state = this.ST_BASE;
  }

  processSetScrollField(b) {
    this.params.push(b);
    if (this.params.length > 1) {
      this.scrollFieldUpper = this.params[0] - 0x20;
      this.scrollFieldLower = this.params[1] - 0x20;
      this.state = this.ST_BASE;
    }
  }

  processWriteCursor(b) {
    this.params.push(b);
    if (this.params.length > 1) {
      this.x = this.params[0] - 0x20;
      this.y = this.params[1] - 0x20;
      this.state = this.ST_BASE;
      this.emulator.renderText(`[[${this.x},${this.y}]]`);
    }
  }

  put(x, y, b) {
    if (x < this.width && y < this.height) {
      this.screen[(y * this.width) + x] = b;
    }
  }

  renderText(data) {
    if (typeof data === "string") {
      let ab = new Uint8Array(data.length);
      for (let i = 0; i < data.length; i++) {
        ab[i] = data.charCodeAt(i) & 0xff;
      }
      data = ab;
    }

    for (let i = 0; i < data.byteLength; i++) {
      let b = data[i];
      switch (this.state) {
      case this.ST_BASE:     this.processBaseChar(b); break;
      case this.ST_RS_1:     this.processRS1(b); break;
      case this.ST_W_CURSOR: this.processWriteCursor(b); break;
      case this.ST_DEF_FN:   this.processDefFn(b); break;
      case this.ST_REPORT:   this.processReport(b); break;
      case this.ST_LOAD:     this.processLoadRAM(b); break;
      case this.ST_X_CHAR:   this.processExtChar(b); break;
      case this.ST_SET_SF:   this.processSetScrollField(b); break;
      case this.ST_RS_DC2:   this.processDC2(b); break;
      }
    }
  }

  setDebug(isDebug) {
    this.emulator.setDebug(isDebug);
  }
}

const downloadFile = (dtc, name, url, dir) => {
  let progressMaxLen = 0;
  let promise = dtc.say(`Download ${name} ...`)
  .then(() => dtc.wget(url, dir, name, (byteCount, contentLength) => {
    let progress = `\r${new Date().toLocaleTimeString()}   Received ${byteCount}`;
    if (contentLength === -1) {
      progress += " bytes";
    }
    else {
      progress += ` of ${contentLength} bytes (${Math.round((byteCount / contentLength) * 100)}%)`;
    }
    if (progress.length > progressMaxLen) progressMaxLen = progress.length;
    process.stdout.write(progress)
  }))
  .then(() => new Promise((resolve, reject) => {
    let progress = `\r`;
    while (progress.length++ < progressMaxLen) progress += " ";
    process.stdout.write(`${progress}\r`);
    resolve();
  }));
  return promise;
};

const isDone = step => {
  return doContinue && completedSteps.indexOf(step) !== -1;
};

const mountVsn = (dtc, vsn) => {
  const m = media[vsn];
  return dtc.say(`Request VSN ${vsn} ...`)
  .then(() => downloadFile(dtc, m.name, m.url, "opt/tapes"))
  .then(() => dtc.say(`Mount VSN ${vsn} on tape unit ...`))
  .then(() => dtc.mount(21, 0, 1, `opt/tapes/${m.name}`));
};

const saveStep = step => {
  completedSteps.push(step);
  fs.writeFileSync(completedStepsPath, completedSteps.join("\n") + "\n");
  return Promise.resolve();
};

const usage = () => {
  process.stderr.write("Usage: node install-nve [-auto][-continue]\n");
  process.stderr.write("  -auto      do NOT update IPR deck to start NOS/VE automatically\n");
  process.stderr.write("  -continue  continue from last step completed in previous run\n");
  process.exit(1);
};

let autoNVE    = true;
let doContinue = false;

for (let i = 2; i < process.argv.length; i++) {
  if (process.argv[i] === "-auto") {
    autoNVE = false;
  }
  else if (process.argv[i] === "-continue") {
    doContinue = true;
  }
  else if (process.argv[i] === "-h" || process.argv[i] === "-help" || process.argv[i] === "--help") {
    usage();
    process.exit(0);
  }
  else {
    usage();
    process.exit(1);
  }
}

const ipAddress = dtc.getHostIpAddress();

if (doContinue && fs.existsSync(completedStepsPath)) {
  completedSteps = fs.readFileSync(completedStepsPath, "utf8").trim().split("\n");
}

//  Determine local country code
const fullLocale  = Intl.DateTimeFormat().resolvedOptions().locale;
const localeInfo  = new Intl.Locale(fullLocale);
const countryCode = localeInfo.region; 

//  Determine hours and minutes offset from GMT and whether daylight savings time is current
const now = new Date();
let   tzOffsetMinutes = now.getTimezoneOffset();
const tzOffsetHours = -(tzOffsetMinutes / 60);
const julyTzOffset = new Date(now.getFullYear(), 6, 1).getTimezoneOffset();
const isTzDST = julyTzOffset === tzOffsetMinutes;
tzOffsetMinutes = tzOffsetMinutes % 30;

term.isTelnetConnection = false;

const filter = new VikingFilter(term.emulator);

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.say("Connected to DtCyber"))
.then(() => dtc.console("idle off"))
.then(() => dtc.console("event_notification on"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => term.connect(`${ipAddress}:6604`))
.then(() => {
  term.emulator     = filter;
  filter.context    = term.emulator.context;
  filter.fontWidth  = term.emulator.fontWidth;
  filter.fontHeight = term.emulator.fontHeight;
  return Promise.resolve();
})
.then(() => term.say("Connected to NOS/VE console"))
.then(() => {
  //
  //  Initiate NOS/VE deadstart and wait for operator intervention
  //
  const stepName = "first deadstart";
  if (isDone(stepName)) return Promise.resolve();
  return dtc.say("Use NVEWAIT to start NVE subsystem and initiate NOS/VE installation ...")
  .then(() => dtc.dsd("NVWAIT."))
  .then(() => term.expect([{ re: /Press NEXT to accept parameters/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Set DCFILE as DCF01 ..."))
  .then(() => term.send("3\r"))
  .then(() => term.expect([{ re: /Enter the DCFILE name/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("DCF01\r"))
  .then(() => term.expect([{ re: /Press NEXT to accept parameters/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Set deadstart device as tape on channel 17(10) ..."))
  .then(() => term.send("5\r"))
  .then(() => term.expect([{ re: /Enter the Channel number/ }])) .then(() => term.sleep(2000))
  .then(() => term.send("17\r"))
  .then(() => term.expect([{ re: /Press NEXT to accept parameters/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Set system device as disk on channel 16(10) ..."))
  .then(() => term.send("10\r"))
  .then(() => term.expect([{ re: /Enter the Channel number/ }]))
  .then(() => term.send("16\r"))
  .then(() => term.expect([{ re: /Press NEXT to accept parameters/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("\r"))
  //
  //  Enter system core commands
  //
  .then(() => term.expect([{ re: /Enter system core commands:/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Enter system core commands ..."))
  .then(() => {
    let promise = term.say(`  settz ${tzOffsetHours} ${tzOffsetMinutes} ${isTzDST}`)
    .then(() => term.send(`settz ${tzOffsetHours} ${tzOffsetMinutes} ${isTzDST}\r`));
    for (const cmd of coreCommands) {
      promise = promise
      .then(() => term.sleep(1000))
      .then(() => term.say(`  ${cmd}`))
      .then(() => term.send(`${cmd}\r`));
    }
  })
  //
  //  Define the physical configuration
  //
  .then(() => term.expect([{ re: /Enter selection, GO/ }])) .then(() => term.sleep(1000))
  .then(() => {
    let promise = term.say("Intervene to change the physical configuration ...")
    .then(() => term.send("1\r"))
    .then(() => term.expect([{ re: /Enter selection or/ }])) .then(() => term.sleep(1000))
    .then(() => term.send("2\r"))
    .then(() => term.expect([{ re: /Press RETURN\/NEXT when ready/ }])) .then(() => term.sleep(1000))
    .then(() => term.send("\r"))
    .then(() => term.expect([{ re: /PCU\// }])) .then(() => term.sleep(1000))
    .then(() => term.send("edipc\r"))
    for (const cmd of pcuCommands) {
      promise = promise
      .then(() => term.expect([{ re: /PCE\// }])) .then(() => term.sleep(1000))
      .then(() => term.say(`  ${cmd}`))
      .then(() => term.send(`${cmd}\r`));
    }
    return promise
    .then(() => term.expect([{ re: /PCU\// }])) .then(() => term.sleep(1000))
    .then(() => term.send("inspc\r"))
    .then(() => term.expect([{ re: /PCU\// }])) .then(() => term.sleep(1000))
    .then(() => term.send("quit\r"))
  })
  //
  //  Define the logical configuration
  //
  .then(() => term.expect([{ re: /Enter selection, GO/ }])) .then(() => term.sleep(1000))
  .then(() => {
    let promise = term.say("Intervene to change the logical configuration ...")
    .then(() => term.send("1\r"));
    for (const cmd of lcuCommands) {
      promise = promise
      .then(() => term.expect([{ re: /LCU\// }])) .then(() => term.sleep(1000))
      .then(() => term.say(`  ${cmd}`))
      .then(() => term.send(`${cmd}\r`));
    }
    return promise;
  })
  //
  //  Install the base NOS/VE 1.8.3 L857 system
  //
  .then(() => term.expect([{ re: /Choose one of the following selections:/ }])) .then(() => term.sleep(2000))
  .then(() => term.say("Install the NOS/VE 1.8.3 base ..."))
  .then(() => term.send("1\r"))
  .then(() => term.expect([{ re: /Enter a menu selection, GO/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("go\r"))
  .then(() => term.expect([{ re: /Requesting installation tape/ }]))
  .then(() => term.say("Load NOS/VE L857 installation tape ..."))
  .then(() => mountVsn(dtc, "5V001A"))
  .then(() => term.say("Wait for base NOS/VE 1.8.3 installation to complete ..."))
  .then(() => term.expect([{ re: /Your system activation choices are:/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("1\r"))
  .then(() => term.expect([{ re: /SYSTEM ACTIVATION COMPLETE/ }])) .then(() => term.sleep(5000))
  .then(() => saveStep(stepName))
  .then(() => term.say("Base NOS/VE 1.8.3 installation complete"));
})
.then(() => {
  //
  //  Create the default NVE family and add users INSTALL and GUEST
  //
  const stepName = "create default family";
  if (isDone(stepName)) return Promise.resolve();
  return dtc.say("Use NVEWAIT to start NVE subsystem and initiate NOS/VE installation ...")
  .then(() => term.say("Create default family NVE ..."))
  .then(() => term.send("create_installation_environment\r"))
  .then(() => term.expect([{ re: /creie\// }])) .then(() => term.sleep(1000))
  .then(() => term.send(`credf fn=nve un=install pw=${utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL")}\r`))
  .then(() => term.expect([{ re: /creie\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Create GUEST user in NVE family ..."))
  .then(() => term.send("admv\r"))
  .then(() => term.expect([{ re: /ADMV\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("usevf :nve.$system.$validations\r"))
  .then(() => term.expect([{ re: /ADMV\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("creu guest\r"))
  .then(() => term.expect([{ re: /CREU\// }])) .then(() => term.sleep(1000))
  .then(() => term.send(`change_login_password npw=${utilities.getPropertyValue(customProps, "PASSWORDS", "GUEST", "GUEST")}\r`))
  .then(() => term.expect([{ re: /CREU\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /ADMV\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Authorize NOS user GUEST for application VEIAF ..."))
  .then(() => dtc.dsd("X.MODVAL(OP=Z)/GUEST,AP=VEIAF"))
  .then(() => saveStep(stepName))
  .then(() => term.say("Default family NVE created"));
})
.then(() => {
  //
  //  Load packing list for NOS/VE L847
  //
  const stepName = "load L847 packing list";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Load packing list for NOS/VE L847 ...")
  .then(() => mountVsn(dtc, "CG019A"))
  .then(() => term.send("inss\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("loapl packing_list_847 'CG019A',,mt9$6250\r"))
  .then(() => dtc.expect([ {re:/CH21,EQ00,UN01 tape unloaded/ }]))
  .then(() => term.send("\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("NOS/VE L847 packing list loaded"));
})
.then(() => {
  //
  //  Load packing list for NOS/VE L826
  //
  const stepName = "load L826 packing list";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Load packing list for NOS/VE L826 ...")
  .then(() => mountVsn(dtc, "1J006A"))
  .then(() => term.send("inss\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("loapl packing_list_826 '1J006A',,mt9$6250\r"))
  .then(() => dtc.expect([ {re:/CH21,EQ00,UN01 tape unloaded/ }]))
  .then(() => term.send("\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("NOS/VE L826 packing list loaded"));
})
.then(() => {
  //
  //  Install software products from NOS/VE L857, L847, and L826
  //
  const stepName = "install software products";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Install software products ...")
  .then(() => term.send("ved tm\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("inss\r"))
  .then(() => {
    let promise = Promise.resolve();
    for (const product of productList) {
      const substepName = `install ${product.name}`;
      if (isDone(substepName)) continue;
      promise = promise
      .then(() => term.say(`--- ${product.name} ---`))
      .then(() => term.sleep(1000))
      .then(() => term.send("\r"))
      .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
      .then(() => term.send(`insp ${product.packingList} ${product.name}\r`))
      .then(() => term.expect([{ re: /\[\[0,6\]\] [^ ]...../ }]))
      .then(() => mountVsn(dtc, filter.getString(1, 6, 6)))
      .then(() => dtc.expect([ {re:/CH21,EQ00,UN01 tape unloaded/ }]))
      .then(() => saveStep(substepName))
      .then(() => dtc.sleep(2000));
    }
    return promise;
  })
  .then(() => term.sleep(1000))
  .then(() => term.send("\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("ved null\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("Software products insalled"));
})
.then(() => {
  //
  //  Install NOS/VE assembler from backup image
  //
  const stepName = "install assembler";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Install NOS/VE Assembler ...")
  .then(() => downloadFile(dtc, "NOSVE_Assembler.tap", "https://www.dropbox.com/scl/fi/odre4729pbmru52m9hu8l/NOSVE_Assembler.tap?rlkey=b000khv547byc2rz0clgy671r&dl=1", "opt/tapes"))
  .then(() => dtc.dsd([
    "[UNLOAD,51.",
    "[!"
  ]))
  .then(() => dtc.mount(13, 0, 1, "opt/tapes/NOSVE_Assembler.tap"))
  .then(() => dtc.sleep(5000))
  .then(() => dtc.say("Copy NOS/VE Assembler installation image to NOS ..."))
  .then(() => dtc.dis([
    "PURGE,NVEASM/NA.",
    "ASSIGN,51,TAPE,LB=KL,F=I,PO=R.",
    "DEFINE,NVEASM.",
    "COPYBR,TAPE,NVEASM."
  ], "NVEASM", 1))
  .then(() => term.say("Copy NOS/VE Assembler installation image from NOS to NOS/VE ..."))
  .then(() => term.send(`change_link_attributes f=cyber u=install pw=${utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL")}\r`))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("get_file nveasm dc=b56\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Install NOS/VE Assembler ..."))
  .then(() => term.send("restore_permanent_files\r"))
  .then(() => term.expect([{ re: /PUR\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("restore_all_files bf=nveasm\r"))
  .then(() => term.expect([{ re: /PUR\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("NOS/VE Assembler insalled"));
})
.then(() => {
  //
  //  Create $SYSTEM.SITE_OS_MAINTENANCE.DEADSTART_COMMANDS.DCFILE
  //
  const stepName = "create dcfile";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Create $SYSTEM.SITE_OS_MAINTENANCE.DEADSTART_COMMANDS.DCFILE ...")
  .then(() => {
    let promise = term.send("colt $system.site_os_maintenance.deadstart_commands.dcfile\r");
    for (const line of dcfile) {
      promise = promise
      .then(() => term.expect([{ re: /ct\?/ }])) .then(() => term.sleep(1000))
      .then(() => term.send(`${line}\r`));
    }
    return promise
    .then(() => term.expect([{ re: /ct\?/ }])) .then(() => term.sleep(1000))
    .then(() => term.send("**\r"))
    .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000));
  })
  .then(() => saveStep(stepName))
  .then(() => term.say("$SYSTEM.SITE_OS_MAINTENANCE.DEADSTART_COMMANDS.DCFILE created"));
})
.then(() => {
  //
  //  Install WEBTERM terminal definition
  //
  const stepName = "install webterm";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Install WEBTERM terminal definition ...")
  .then(() => term.say("Upload WEBTERM source to NOS ..."))
  .then(() => {
    const text = fs.readFileSync("files/WEBTRM-NVE.txt", "utf8");
    const options = {
      username: "INSTALL",
    };
    options.password = utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL");
    return dtc.putFile("WBTRMVE/IA", text, options);
  })
  .then(() => term.say("Copy WEBTERM source to NOS/VE ..."))
  .then(() => term.send("get_file webterm_tdu wbtrmve\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Compile WEBTERM source ..."))
  .then(() => term.send("deft webterm_tdu\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Add WEBTERM to $SYSTEM.TDU.TERMINAL_DEFINITIONS ..."))
  .then(() => term.send("creol\r"))
  .then(() => term.expect([{ re: /COL\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("addm ($system.tdu.terminal_definitions terminal_definitions)\r"))
  .then(() => term.expect([{ re: /COL\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("genl $system.tdu.terminal_definitions.$next\r"))
  .then(() => term.expect([{ re: /COL\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("WEBTERM terminal definition installed"));
})
.then(() => {
  //
  //  Activate Mail/VE and create mailboxes for INSTALL and GUEST
  //
  const stepName = "activate mail/ve";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Install and activate Mail/VE ...")
  .then(() => term.send("crecle $system.mailve_v2.maintenance.command_library\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.send(`insm mta='MTA${utilities.getMachineId(dtc)}' pd='${utilities.getHostId(dtc)}' ad=' ' c='${countryCode}'\r`))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("activate_mailve\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("colt $system.prologs_and_epilogs.job_activation_epilog.$eoi\r"))
  .then(() => term.expect([{ re: /ct\?/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("activate_mailve\r"))
  .then(() => term.expect([{ re: /ct\?/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("**\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Create mailboxes for INSTALL and GUEST ..."))
  .then(() => term.send("admm\r"))
  .then(() => term.expect([{ re: /Admm\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("crem\r"))
  .then(() => term.expect([{ re: /Crem\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("selu u=install f=nve\r"))
  .then(() => term.expect([{ re: /Crem\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("seta pn='Install'\r"))
  .then(() => term.expect([{ re: /Crem\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /Admm\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("crem\r"))
  .then(() => term.expect([{ re: /Crem\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("selu u=guest f=nve\r"))
  .then(() => term.expect([{ re: /Crem\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("seta pn='Guest'\r"))
  .then(() => term.expect([{ re: /Crem\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /Admm\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("Mail/VE installed and activated"));
})
.then(() => {
  //
  //  Install the L857 BCU
  //
  const stepName = "load bcu packing list";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Install L857 BCU ...")
  .then(() => term.say("Load packing list for NOS/VE L857 BCU ..."))
  .then(() => mountVsn(dtc, "AS002V"))
  .then(() => term.send("inss\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("loapl packing_list_bcu 'AS002V' t=mt9$6250 uv=false\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Load BCU addendum document ..."))
  .then(() => term.send("insc packing_list_bcu pre_install_sw io=immediate\r"))
  .then(() => dtc.expect([ {re:/CH21,EQ00,UN01 tape unloaded/ }]))
  .then(() => term.send("\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Apply corrections to INSTALLATION_TOOLS ..."))
  .then(() => dtc.mount(21, 0, 1, "opt/tapes/nve857-bcu-as002v.tap"))
  .then(() => term.send("insc packing_list_bcu p=installation_tools io=immediate\r"))
  .then(() => dtc.expect([ {re:/CH21,EQ00,UN01 tape unloaded/ }]))
  .then(() => term.send("\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("L857 BCU packing list loaded"));
})
.then(() => {
  //
  //  Label a new deadstart tape
  //
  const stepName = "label new deadstart tape";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Label a new deadstart tape ...")
  .then(() => term.say("Unmount current NOS/VE deadstart tape ..."))
  .then(() => dtc.unmount(21, 0, 0))
  .then(() => term.sleep(2000))
  .then(() => term.say("Label a new deadstart tape ..."))
  .then(() => {
    return dtc.mount(21, 0, 1, `tapes/nve857ds.new.tap`, true)
    .then(() => dtc.sleep(5000))
    .then(() => term.send(`initv tape2 '${dsTapeLabel}' mt9$6250\r`))
    .then(() => term.expect([{ re: /Enter choice or/ }])) .then(() => term.sleep(1000))
    .then(() => term.send("1\r"))
    .then(() => dtc.expect([ {re:/CH21,EQ00,UN01 tape unloaded/ }]));
  })
  .then(() => saveStep(stepName))
  .then(() => term.say("New deadstart tape labeled"));
})
.then(() => {
  //
  //  Install the L857 BCU
  //
  const stepName = "apply corrections";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Apply all corrections from BCU tape ...")
  .then(() => term.send("\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("ved tm\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("inss\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => mountVsn(dtc, "AS002V"))
  .then(() => term.send(`appac packing_list_bcu nvdt=('${dsTapeLabel}','${dsTapeLabel}',mt9$6250) io=deferred\r`))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Wait for request to mount new deadstart tape ..."))
  .then(() => term.expect([{ re: new RegExp(`\\[\\[0,6\\]\\] ${dsTapeLabel}`) }]))
  .then(() => term.send("\r"))
  .then(() => term.expect([{ re: /INSS\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Remount and write new deadstart tape ..."))
  .then(() => dtc.mount(21, 0, 0, "tapes/nve857ds.new.tap", true))
  .then(() => term.say("Wait for deferred BCU installation to complete ..."))
  .then(() => dtc.expect([ {re:/CH21,EQ00,UN00 tape unloaded/ }]))
  .then(() => term.say("New deadstart tape unloaded ..."))
  .then(() => term.send("ved null\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("Corrections applied"));
})
.then(() => {
  //
  //  Terminate NOS/VE and wait for NVE to drop
  //
  const stepName = "terminate nos/ve";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Terminate NOS/VE and wait for NVE subsystem to drop ...")
  .then(() => term.send("terminate_system\r"))
  .then(() => term.expect([{ re: /System TERMINATED via OPERATOR COMMAND/ }]))
  .then(() => term.sleep(30000))
  .then(() => saveStep(stepName))
  .then(() => term.say("NOS/VE terminated and NVE dropped"));
})
.then(() => {
  //
  //  Deadstart NOS/VE using new deadstart tape
  //
  const stepName = "deadstart nos/ve from new tape";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Deadstart NOS/VE using new tape ...")
  .then(() => term.say("Unmount current NOS/VE deadstart tape ..."))
  .then(() => dtc.unmount(21, 0, 0))
  .then(() => term.sleep(2000))
  .then(() => term.say("Mount new deadstart tape ..."))
  .then(() => dtc.mount(21, 0, 0, "tapes/nve857ds.new.tap"))
  .then(() => term.say("Initiate NOS/VE deadstart using NVEWAIT ..."))
  .then(() => dtc.dsd("NVWAIT."))
  .then(() => term.expect([{ re: /Press NEXT to accept parameters/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("\r"))
  .then(() => term.say("Wait for prompt to enter system core commands ..."))
  .then(() => term.expect([{ re: /Enter system core commands:/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("System core commands:"))
  .then(() => term.say("  setsa job_recovery_option 3"))
  .then(() => term.send("setsa job_recovery_option 3\r"))
  .then(() => term.sleep(1000))
  .then(() => term.say("  go"))
  .then(() => term.send("go\r"))
  .then(() => term.say("Wait for PCU/LCU intervention prompt ..."))
  .then(() => term.expect([{ re: /Enter selection, GO/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("go\r"))
  .then(() => term.say("Wait for prompt to install deferred products ..."))
  .then(() => term.expect([{ re: /Enter selection or/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Install deferred products ..."))
  .then(() => term.send("4\r"))
  .then(() => term.expect([{ re: /Enter selection or/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Activate the system for production ..."))
  .then(() => term.send("1\r"))
  .then(() => term.expect([{ re: /SYSTEM ACTIVATION COMPLETE/ }])) .then(() => term.sleep(1000))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("Deadstart from new tape complete"));
})
.then(() => {
  //
  //  Establish disk-based system
  //
  const stepName = "establish disk-based system";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Establish disk-based system ...")
  .then(() => term.send("maids\r"))
  .then(() => term.expect([{ re: /maids\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Generate VE deadstart catalog ..."))
  .then(() => term.send("genvdc\r"))
  .then(() => term.expect([{ re: /maids\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Establish disk-based system ..."))
  .then(() => term.send("estdbs $system.site_os_maintenance.l857aa.deadstart_catalog\r"))
  .then(() => term.say("Wait for completion message ..."))
  .then(() => term.expect([{ re: /Disk based system complete/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Commit new system ..."))
  .then(() => term.send("comns\r"))
  .then(() => term.expect([{ re: /maids\// }])) .then(() => term.sleep(1000))
  .then(() => term.send("quit\r"))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => term.say("Terminate NOS/VE to prepare for activating disk-based deadstart ..."))
  .then(() => term.send("terminate_system\r"))
  .then(() => term.expect([{ re: /System TERMINATED via OPERATOR COMMAND/ }]))
  .then(() => term.say("Wait for NVE subsystem to drop ..."))
  .then(() => term.sleep(30000))
  .then(() => saveStep(stepName))
  .then(() => term.say("Disk-based system established"));
})
.then(() => {
  //
  //  Terminate NOS/VE, re-deadstart using NVEWAIT, and switch to disk-based deadstart
  //
  const stepName = "activate disk deadstart";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Start NVE subsystem to activate disk-based NOS/VE deadstart ...")
  .then(() => dtc.dsd("NVWAIT."))
  .then(() => term.expect([{ re: /Press NEXT to accept parameters/ }])) .then(() => term.sleep(2000))
  .then(() => term.say("Set OS Location to Default Disk ..."))
  .then(() => term.send("1\r"))
  .then(() => term.expect([{ re: /Enter OS location:/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("d\r"))
  .then(() => term.expect([{ re: /Press NEXT to accept parameters/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Set DCFILE name to DCF01 ..."))
  .then(() => term.send("3\r"))
  .then(() => term.expect([{ re: /Enter the DCFILE name/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("DCF01\r"))
  .then(() => term.expect([{ re: /Press NEXT to accept parameters/ }])) .then(() => term.sleep(1000))
  .then(() => term.say("Initiate disk-based deadstart ..."))
  .then(() => term.send("\r"))
  .then(() => term.expect([{ re: /Enter system core commands:/ }])) .then(() => term.sleep(1000))
  .then(() => term.send("auto\r"))
  .then(() => term.say("Wait for NOS/VE deadstart to complete ..."))
  .then(() => term.expect([{ re: /SYSTEM ACTIVATION COMPLETE/ }])) .then(() => term.sleep(1000))
  .then(() => term.expect([{ re: /sou\// }])) .then(() => term.sleep(1000))
  .then(() => saveStep(stepName))
  .then(() => term.say("Disk-based deadstart activated"));
})
.then(() => {
  //
  //  Rename deadstart tape, saving previous one
  //
  const stepName = "rename nve deadstart tape";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Rename NOS/VE deadstart tape ...")
  .then(() => {
    const creationDate = new Date(fs.statSync("tapes/nve857ds.tap").birthtime);
    const newName = `tapes/nve857ds-${creationDate.toISOString().replaceAll(':', '')}.tap`;
    fs.renameSync("tapes/nve857ds.tap", newName);
    fs.renameSync("tapes/nve857ds.new.tap", "tapes/nve857ds.tap");
    return term.say(`Rename tapes/nve857ds.tap to ${newName}`)
    .then(() => term.say("Rename tapes/nve857ds.new.tap to tapes/nve857ds.tap"));
  })
  .then(() => saveStep(stepName))
  .then(() => term.say("Disk-based deadstart activated"));
})
.then(() => {
  //
  //  Update cyber.ovl to remove tape image references obviated by disk-based deadstart
  //  of both NOS and NOS/VE
  //
  const stepName = "update cyber.ovl";
  if (isDone(stepName)) return Promise.resolve();
  return term.say("Update cyber.ovl ...")
  .then(() => {
    let inpLines = fs.readFileSync("cyber.ovl", "utf8").split("\n");
    let outLines = [];
    let isTapeDefnsWritten = false;
    for (const line of inpLines) {
      if (line.startsWith("MT679,") && isTapeDefnsWritten === false) {
        outLines.push("MT679,0,0,13");
        outLines.push("MT679,0,1,13");
        outLines.push("MT679,0,2,13");
        outLines.push("MT679,0,3,13");
        outLines.push("MT679,0,0,21");
        outLines.push("MT679,0,1,21");
        isTapeDefnsWritten = true;
      }
      if (/^MT679,0,[0123],13/.test(line) || /^MT679,0,[01],21/.test(line)) continue;
      outLines.push(line);
    }
    fs.writeFileSync("cyber.ovl", outLines.join("\n"));
    return Promise.resolve();
  })
  .then(() => saveStep(stepName));
})
.then(() => {
  //
  //  If autoNVE is true, edit NOS IPRD01 to ENABLE NVE
  //
  const stepName = "enable NVE subsystem";
  if (isDone(stepName)) return Promise.resolve();
  if (autoNVE) {
    let record = [];
    return dtc.say("Edit IPRD01 ...")
    .then(() => utilities.getSystemRecord(dtc, "IPRD01"))
    .then(iprd01 => {
      let lines = [];
      for (const line of iprd01.split("\n")) {
        if (line.startsWith("DISABLE,NVE,")) {
          lines.push(`ENABLE,${line.substring(8)}`);
        }
        else if (line.length > 0) {
          lines.push(line);
        }
      }
      record.push(lines.join("\n") + "\n");
    })
    .then(() => utilities.updateProductRecords(dtc, record))
    .then(() => dtc.say("Make new deadstart tape ..."))
    .then(() => dtc.dsd([
      "[UNLOAD,50.",
      "[UNLOAD,51.",
      "[!"
    ]))
    .then(() => dtc.sleep(3000))
    .then(() => dtc.mount(13, 0, 0, "tapes/ds.tap"))
    .then(() => dtc.mount(13, 0, 1, "tapes/newds.tap", true))
    .then(() => dtc.sleep(5000))
    .then(() => dtc.say("Run job to write new deadstart tape ..."))
    .then(() => dtc.runJob(12, 4, "decks/make-ds-tape.job", [50, 51]))
    .then(() => dtc.say("New deadstart tape created: tapes/newds.tap"))
    .then(() => dtc.say("Install new deadstart image on disk"))
    .then(() => dtc.dsd([
      "[UNLOAD,51.",
      "[!"
    ]))
    .then(() => dtc.sleep(3000))
    .then(() => dtc.mount(13, 0, 1, "tapes/newds.tap"))
    .then(() => dtc.dis([
      "ASSIGN,51,TAPE,LB=KU,F=I,PO=R.",
      "COPYEI,TAPE,DS.",
      "UNLOAD,TAPE.",
      "REWIND,DS.",
      "INSTALL,DS,EQ10."
    ], "DSINST"))
    .then(() => dtc.sleep(5000))
    .then(() => {
      const creationDate = new Date(fs.statSync("tapes/ds.tap").birthtime);
      const newName = `tapes/ds-${creationDate.toISOString().replaceAll(':', '')}.tap`;
      fs.renameSync("tapes/ds.tap", newName);
      fs.renameSync("tapes/newds.tap", "tapes/ds.tap");
      return term.say(`Rename tapes/ds.tap to ${newName}`)
      .then(() => term.say("Rename tapes/newds.tap to tapes/ds.tap"));
    })
    .then(() => saveStep(stepName))
    .then(() => term.say("NVE subsystem enabled"));
  }
  else {
    return saveStep(stepName);
  }
})
.then(() => term.say(""))
.then(() => term.say("--- NOS/VE Installation Complete ---"))
.then(() => term.say(""))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
