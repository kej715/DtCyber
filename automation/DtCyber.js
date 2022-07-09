const bz2 = require("unbzip2-stream");
const child_process = require("child_process");
const extract = require("extract-zip");
const fs = require("fs");
const https = require("https");
const net = require("net");

class DtCyberStreamMgr {

  constructor() {
    this.receivedData = Buffer.alloc(0);
  }

  appendData(data) {
    this.receivedData = Buffer.concat([this.receivedData, data]);
    if (typeof this.consumer === "function") {
      if (this.consumer(this.receivedData) === false) {
        this.endConsumer();
      }
      this.clearInputStream();
    }
  }

  clearInputStream() {
    this.receivedData = Buffer.alloc(0);
  }

  endConsumer() {
    delete this.consumer;
  }

  setOutputStream(stream) {
    this.outputStream = stream;
  }

  startConsumer(consumer) {
    this.consumer = consumer;
    if (this.receivedData.length > 0) {
      if (consumer(this.receivedData) === false) {
        this.endConsumer();
      }
      this.clearInputStream();
    }
  }

  write(data) {
    this.outputStream.write(data);
  }
}

class PrinterStreamMgr {

  constructor() {
    this.buffer = Buffer.alloc(64*1024);
  }

  endConsumer() {
    delete this.consumer;
    this.watcher.close();
  }

  openPrinter(printerFile, callback) {
    const me = this;
    this.printerFile = printerFile;
    fs.open(printerFile, (err, file) => {
      if (err === null) {
        me.file = file;
      }
      if (typeof callback === "function") {
        callback(err, file);
      }
    });
  }

  startConsumer(consumer) {
    const me = this;
    this.consumer = consumer;
    fs.fstat(this.file, (err, stats) => {
      me.position = stats.size;
      this.watcher = fs.watch(this.printerFile, {persistent:false, encoding:"utf8"}, (eventType, filename) => {
        if (eventType === "change") {
          fs.read(me.file, me.buffer, 0, me.buffer.length, me.position, (err, bytesRead, buffer) => {
            if (bytesRead > 0) {
              me.position += bytesRead;
              if (me.consumer(buffer.subarray(0, bytesRead)) === false) {
                me.endConsumer();
              }
            }
          });
        }
      });
    });
  }
}

class DtCyber {

  constructor() {
    this.streamMgrs = {};
  }

  attachPrinter(printerFile) {
    const me = this;
    this.streamMgrs.printer = new PrinterStreamMgr();
    return new Promise((resolve, reject) => {
      me.streamMgrs.printer.openPrinter(printerFile, (err, file) => {
        if (err === null) {
          resolve(me.streamMgrs.printer);
        }
        else {
          reject(err);
        }
      });
    });
  }

  bunzip2(srcPath, dstPath) {
    const me = this;
    return new Promise((resolve, reject) => {
      try {
        let ws = fs.createWriteStream(dstPath);
        ws.on("close", () => {
          resolve(dstPath);
        });
        fs.createReadStream(srcPath).pipe(bz2()).pipe(ws);
      }
      catch(err) {
        reject(err);
      }
    });
  }

  connect(port) {
    const me = this;
    this.isExitOnEnd = true;
    this.streamMgrs.dtCyber = new DtCyberStreamMgr();
    this.connectDeadline = Date.now() + 2000;
    const doConnect = (callback) => {
      me.socket = net.createConnection({port:port}, () => {
        callback(null);
      });
      me.streamMgrs.dtCyber.setOutputStream(me.socket);
      me.socket.on("data", data => {
        me.streamMgrs.dtCyber.appendData(data);
      });
      me.socket.on("end", () => {
        if (me.isExitOnEnd) {
          console.log(`${new Date().toLocaleTimeString()} DtCyber disconnected`);
          process.exit(0);
        }
      });
      me.socket.on("error", err => {
        const now = Date.now();
        if (now >= me.connectDeadline) {
          callback(err);
        }
        else {
          setTimeout(() => {
            doConnect(callback);
          }, 500);
        }
      });
    };
    return new Promise((resolve, reject) => {
      doConnect(err => {
        if (err === null) {
          resolve(me.streamMgrs.dtCyber);
        }
        else {
          reject(err);
        }
      });
    });
  }

  console(command) {
    this.send(command);
    return this.expect([ {re:/Operator> /} ]);
  }

  dis(commands, ui) {
    const me = this;
    let list = [`SUI,${typeof ui === "undefined" ? "377777" : ui.toString()}.`];
    if (Array.isArray(commands)) {
      list = list.concat(commands);
    }
    else {
      list.push(commands);
    }
    list.push("DROP.");
    return this.dsd("X.DIS.")
    .then(() => me.sleep(1000))
    .then(() => me.dsd(list))
    .then(() => me.sleep(1000));
  }

  disconnect() {
    const me = this;
    this.isExitOnEnd = false;
    return new Promise((resolve, reject) => {
      me.socket.end(() => {
        me.socket.destroy();
        resolve();
      });
    });
  }

  dsd(commands) {
    if (typeof commands === "string") commands = [commands];
    const me = this;
    let promise = Promise.resolve();
    for (const command of commands) {
      promise = promise.then(() => me.console(`e ${command}`))
    }
    return promise;
  }

  exec(command, args) {
    return new Promise((resolve, reject) => {
      const child = child_process.spawn(command, args, {
        shell:true,
        stdio:["pipe", process.stdout, process.stderr]
      });
      child.on("exit", (code, signal) => {
        if (signal !== null) {
          reject(new Error(`${command} exited due to signal ${signal}`));
        }
        else if (code === 0) {
          resolve();
        }
        else {
          reject(new Error(`${command} exited with status ${code}`));
        }
      });
      child.on("error", err => {
        reject(err);
      });
    });
  }

  expect(patterns, strmId) {
    const me = this;
    return new Promise((resolve, reject) => {
      let str = "";
      let mgr = me.getStreamMgr(strmId);
      mgr.startConsumer(data => {
        if (str.length > 200) {
          str = str.substring(200);
        }
        str += data;
        for (let pattern of patterns) {
          if (str.match(pattern.re)) {
            if (typeof pattern.fn === "function") {
              try {
                pattern.fn();
                resolve();
              }
              catch(err) {
                reject(err);
              }
            }
            else if (typeof pattern.fn === "string" && pattern.fn === "continue") {
              return true;
            }
            else if (typeof pattern.fn !== "undefined") {
              reject(pattern.fn);
            }
            else {
              resolve();
            }
            return false;
          }
        }
        return true;
      });
    });
  }

  getStreamMgr(strmId) {
    if (typeof strmId === "undefined") strmId = "dtCyber";
    return this.streamMgrs[strmId];
  }

  loadJob(ch, eq, deck) {
    const cmd = `lc ${ch},${eq},${deck}`;
    this.send(cmd);
    return this.expect([
      {re:/Failed to/,                        fn:new Error(`Failed to load job; ${cmd}`)},
      {re:/Not enough or invalid parameters/, fn:new Error(`Not enough or invalid parameters: ${cmd}`)},
      {re:/Invalid/,                          fn:new Error(`Invalid: ${cmd}`)},
      {re:/Input tray full/,                  fn:new Error(`Input tray full: ${cmd}`)},
      {re:/Operator> /}
    ]);
  }

  mount(ch, eq, unit, tape, ring) {
    const cmd = `lt ${ch},${eq},${unit},${(typeof ring !== "undefined" && ring === true) ? "w" : "r"},${tape}`;
    this.send(cmd);
    return this.expect([
      {re:/Failed to open/,                   fn:new Error(`Failed to open ${tape}`)},
      {re:/Not enough or invalid parameters/, fn:new Error(`Not enough or invalid parameters: ${cmd}`)},
      {re:/Invalid/,                          fn:new Error(`Invalid: ${cmd}`)},
      {re:/Operator> /}
    ]);
  }

  runJob(ch, eq, deck, jobParams) {
    const me = this;
    return new Promise((resolve, reject) => {
      try {
        const lines = fs.readFileSync(deck, {encoding:"utf8"}).split("\n");
        let jobName = null;
        for (let line of lines) {
          if (!line.match(/^~/)) {
            jobName = /^([A-Z0-9$]+)/.exec(line)[1];
            break;
          }
        }
        if (jobName !== null) {
          if (Array.isArray(jobParams)) {
            deck = `${deck},${jobParams.join(",")}`;
          }
          else if (typeof jobParams === "string") {
            deck = `${deck},${jobParams}`;
          }
          me.loadJob(ch, eq, deck)
          .then(() => me.waitJob(jobName))
          .then(() => { resolve(); })
          .catch(err => { reject(err); });
        }
        else {
          reject(new Error(`${deck}: invalid card deck`));
        }
      }
      catch(err) {
        reject(err);
      }
    });
  }

  say(message) {
    console.log(`${new Date().toLocaleTimeString()} ${message}`);
    return Promise.resolve();
  }

  send(command, strmId) {
    const mgr = this.getStreamMgr(strmId);
    mgr.write(`${command}\n`);
  }

  shutdown() {
    const me = this;
    return me.say("Starting shutdown sequence ...")
    .then(() => me.dsd("[UNLOCK."))
    .then(() => me.dsd("CHE"))
    .then(() => me.sleep(8000))
    .then(() => me.dsd("STEP."))
    .then(() => me.sleep(2000))
    .then(() => me.console("shutdown"));
  }

  sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  start(command, args) {
    const me = this;
    this.streamMgrs.dtCyber = new DtCyberStreamMgr();
    return new Promise((resolve, reject) => {
      const child = child_process.spawn(command, args, {
        stdio:["pipe", "pipe", process.stderr]
      });
      me.streamMgrs.dtCyber.setOutputStream(child.stdin);
      child.on("spawn", () => {
        resolve(me.streamMgrs.dtCyber);
      });
      child.on("exit", (code, signal) => {
        const d = new Date();
        if (signal !== null) {
          console.log(`${d.toLocaleTimeString()} DtCyber exited due to signal ${signal}`);
          process.exit(1);
        }
        else if (code === 0) {
          console.log(`${d.toLocaleTimeString()} DtCyber exited normally`);
          process.exit(0);
        }
        else {
          console.log(`${d.toLocaleTimeString()} DtCyber exited with status ${code}`);
          process.exit(code);
        }
      });
      child.on("error", err => {
        reject(err);
      });
      child.stdout.on("data", (data) => {
        me.streamMgrs.dtCyber.appendData(data);
      });
    });
  }

  unmount(ch, eq, unit) {
    const cmd = `ut ${ch},${eq},${unit}`;
    this.send(cmd);
    return this.expect([
      {re:/not loaded> /,                     fn:"continue"},
      {re:/Not enough or invalid parameters/, fn:new Error(`Not enough or invalid parameters: ${cmd}`)},
      {re:/Invalid/,                          fn:new Error(`Invalid: ${cmd}`)},
      {re:/Operator> /}
    ]);
  }

  unzip(srcPath, dstDir) {
    return extract(srcPath, { dir: fs.realpathSync(dstDir) });
  }

  waitJob(jobName) {
    return this.expect([
      {re:new RegExp(`\\*\\*\\* ${jobName} FAILED`, "i"), fn:new Error(`${jobName} failed`)},
      {re:new RegExp(`\\*\\*\\* ${jobName} COMPLETE`, "i")}
    ], "printer");
  }

  wget(url, cacheDir, filename) {
    const me = this;
    return new Promise((resolve, reject) => {
      const pathname = new URL(url).pathname;
      if (typeof filename === "undefined") {
        const li = pathname.lastIndexOf("/");
        filename = pathname.substring((li >= 0) ? li + 1 : 0);
      }
      const cachePath = `${cacheDir}/${filename}`;
      const stat = fs.statSync(cachePath, {throwIfNoEntry:false});
      if (typeof stat !== "undefined" && (Date.now() - stat.ctimeMs) < (24*60*60*1000)) {
        resolve(cachePath);
      }
      else {
        const strm = fs.createWriteStream(cachePath, {mode:0o644});
        https.get(url, res => {
          res.on("data", data => {
            strm.write(data);
          });
          res.on("end", () => {
            strm.end();
            switch (res.statusCode) {
            case 200:
              resolve(cachePath);
              break;
            case 301:
            case 302:
              fs.unlinkSync(cachePath);
              let location = res.headers.location;
              if (location.startsWith("/")) {
                const pi = url.indexOf(pathname);
                location = `${url.substring(0, pi)}${location}`;
              }
              me.wget(location, cacheDir, filename)
              .then(path => { resolve(path); })
              .catch(err => { reject(err); });
              break;
            default:
              fs.unlinkSync(cachePath);
              reject(new Error(`${res.statusCode}: failed to get ${url}`));
              break;
            }
          });
        }).on("error", err => {
          reject(err);
        });
      }
    });
  }
}

module.exports = DtCyber;
