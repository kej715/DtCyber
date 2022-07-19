const bz2 = require("unbzip2-stream");
const child_process = require("child_process");
const extract = require("extract-zip");
const fs = require("fs");
const https = require("https");
const net = require("net");

/*
 * DtCyberStreamMgr
 *
 * This class manages the input and output streams associated with a connection
 * to the DtCyber operator interface.
 */
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

/*
 * PrinterStreamMgr
 *
 * This class manages the input stream associated with the file to which
 * the DtCyber line printer device writes. It uses a file watcher to
 * detect changes in the file and reads newly appended data, effectively
 * performing a "tail -f" function.
 */
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

/*
 * DtCyber
 *
 * This is the main class of the module. It provides a collection of methods facilitating
 * interaction with DtCyber.
 */
class DtCyber {

  constructor() {
    this.streamMgrs = {};
  }

  /*
   * attachPrinter
   *
   * Instantiates a PrinterStreamMgr to begin monitoring a file to which DtCyber's line printer
   * device is connected.
   *
   * Arguments:
   *   printerFile - name of file to which DtCyber's printer is connected
   *
   * Returns:
   *   A promise that is resolved when the printer file is open and ready to be monitored
   */
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

  /*
   * bunzip2
   *
   * Decompress a file that was comppressed using the BZIP2 algorithm.
   *
   * Arguments:
   *   srcPath - pathname of compressed input file
   *   dstPath - pathname of decmpressed output file to be created
   *
   * Returns:
   *   A promise that is resolved when decompression has completed
   */
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

  /*
   * connect
   *
   * Create a connection to the DtCyber operator interface. Looks for a cyber.ini in
   * the current working directory or its parent, parses the file for a set_operator_port
   * command, and attempts to connect to the port number defined by that command. Creates
   * an instance of DtCyberStreamMgr to manage the connection that is established.
   *
   * Returns:
   *   A promise that is resolved when the connection has been established
   */
  connect() {
    const me = this;
    this.isExitOnEnd = true;
    if (typeof this.operatorPort === "undefined") {
      let path = null;
      for (const p of ["./cyber.ini", "../cyber.ini"]) {
        if (fs.existsSync(p)) {
          path = p;
          break;
        }
      }
      if (path === null) {
        throw new Error("cyber.ini not found");
      }
      const lines = fs.readFileSync(path, "utf8").split("\n");
      for (const line of lines) {
        let result = /^\s*set_operator_port\s+([0-9]+)/.exec(line);
        if (result !== null) {
          this.operatorPort = parseInt(result[1]);
          break;
        }
      }
      if (typeof this.operatorPort === "undefined") {
        throw new Error(`Operator port not found in ${path}`);
      }
    }
    this.streamMgrs.dtCyber = new DtCyberStreamMgr();
    this.connectDeadline = Date.now() + 2000;
    const doConnect = (callback) => {
      me.socket = net.createConnection({port:me.operatorPort}, () => {
        callback(null);
      });
      me.streamMgrs.dtCyber.setOutputStream(me.socket);
      me.socket.on("data", data => {
        me.streamMgrs.dtCyber.appendData(data);
      });
      me.socket.on("end", () => {
        if (me.isExitOnEnd
            && (typeof me.isExitAfterShutdown === "undefined" || me.isExitAfterShutdown === true)) {
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

  /*
   * console
   *
   * Sends a command to the DtCyber operator interface and waits for the interface to
   * indicate that the command is complete by prompting for another command.
   *
   * Arguments:
   *   command - the command to send to the operator interface
   *
   * Returns:
   *   A promise that is resolved when the command has completed
   */
  console(command) {
    this.send(command);
    return this.expect([ {re:/Operator> /} ]);
  }

  /*
   * dis
   *
   * Call DIS, provide it with a sequence of commands to execute, and then drop DIS. 
   *
   * Arguments:
   *   commands - a string representing a single command to execute, or
   *              an array of strings representing a sequence of commands
   *              to execute. 
   *   ui       - user index under which to execute the sequence of commands.
   *              This argument is optional. If omitted, user index 377777
   *              is used.
   *
   * Returns:
   *   A promise that is resolved when the sequence of commands has completed and
   *   DIS has dropped
   */
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

  /*
   * disconnect
   *
   * Disconnect from the DtCyber operator interface, usually with intention to
   * perform work after disconnecting and possibly re-connecting later.
   *
   * Returns:
   *   A promise that is resolved when disconnection has completed
   */
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

  /*
   * dsd
   *
   * Execute a sequence of DSD commands.
   *
   * Arguments:
   *   commands - a string representing a single command to execute, or
   *              an array of strings representing a sequence of commands
   *              to execute.
   *
   * Returns:
   *   A promise that is resolved when the sequence of commands has completed
   */
  dsd(commands) {
    if (typeof commands === "string") commands = [commands];
    const me = this;
    let promise = Promise.resolve();
    for (const command of commands) {
      promise = promise
      .then(() => me.sleep(500))
      .then(() => me.console(`e ${command}`))
    }
    return promise;
  }

  /*
   * exec
   *
   * Spawn and optionally detach a subprocess using the Node.js
   * child_process.spawn API.
   *
   * Arguments:
   *   command - the command to execute, usually "node"
   *   args    - array of arguments for the command
   *   options - optional object providing child_process.spawn options
   *             If omitted, the default options are:
   *               {shell: true, stdio: ["pipe", process.stdout, process.stderr]}
   *
   * Returns:
   *   A promise that is resolved when a detached process has started, or when
   *   a non-detached process has exited normally
   */
  exec(command, args, options) {
    if (typeof options === "undefined") {
      options = {
        shell:true,
        stdio:["pipe", process.stdout, process.stderr]
      };
    }
    return new Promise((resolve, reject) => {
      const child = child_process.spawn(command, args, options);
      if (options.detached) {
        if (typeof options.unref === "undefined" || options.unref === true) {
          child.unref();
        }
        resolve(child);
      }
      else {
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
      }
    });
  }

  /*
   * expect
   *
   * Match the contents of a stream against a set of patterns and take
   * specified actions when matches occur.
   *
   * Arguments:
   *   patterns - a array of objects representing patterns and associated
   *              actions to be taken when matches occur. Each object has
   *              these fields:
   *                re: regular expression representing a pattern to
   *                    be applied to the stream
   *                fn: if string "continue", other patterns in the array
   *                    continue to be applied. This provides a way to 
   *                    recognize an intermediate pattern in the stream
   *                    and continue looking for other patterns. 
   *                    if a function, the function is executed and a match
   *                    is indicated.
   *                    if ommitted, a match is indicated.
   *                    otherwise, an error is indicated
   *   strmId   - optional string identifying the stream to which patterns
   *              are applied. If omitted, the default is "dtCyber".
   *
   * Returns:
   *   A promise that is resolved when a match is indicated.
   */
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

  /*
   * getStreamMgr
   *
   * Look up a registered stream manager by identifier.
   *
   * Arguments:
   *   strmId - stream identifier, usually "dtCyber" or "printer".
   *            If omitted, the default is "dtCyber".
   *
   * Returns:
   *   The stream manager object registered with the specified id.
   */
  getStreamMgr(strmId) {
    if (typeof strmId === "undefined") strmId = "dtCyber";
    return this.streamMgrs[strmId];
  }

  /*
   * loadJob
   *
   * Load a card deck on a specified card reader.
   *
   * Arguments:
   *   ch   - channel number of the channel to which the reader is attached
   *   eq   - equipment number of the reader on the channel
   *   deck - pathname of the card deck to be loaded
   *
   * Returns:
   *   A promise that is resolved when the card deck has been loaded.
   */
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

  /*
   * mount
   *
   * Mount a tape image on a specified tape drive.
   *
   * Arguments:
   *   ch   - channel number of the channel to which the tape drive is attached
   *   eq   - equipment number of the tape drive on the channel
   *   unit - unit number of the tape drive
   *   tape - pathname of the tape image to be mounted
   *   ring - true if tape to be mounted as writable
   *          If omitted, the default is false
   *
   * Returns:
   *   A promise that is resolved when the tape has been mounted.
   */
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

  /*
   * runJob
   *
   * Load a card deck on a specified card reader and wait for the associated
   * job to complete. See description of waitJob for information about the
   * expected format of the card deck.
   *
   * Arguments:
   *   ch        - channel number of the channel to which the reader is attached
   *   eq        - equipment number of the reader on the channel
   *   deck      - pathname of the card deck to be loaded
   *   jobParams - optional parameters substituted into the card deck
   *
   * Returns:
   *   A promise that is resolved when the job has completed.
   */
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

  /*
   * say
   *
   * Display a message prefaced by a timestamp to the user.
   *
   * Arguments:
   *   message - the message to be displayed
   *
   * Returns:
   *   A promise that is resolved when the message has been displayed.
   */
  say(message) {
    console.log(`${new Date().toLocaleTimeString()} ${message}`);
    return Promise.resolve();
  }

  /*
   * send
   *
   * Send a command to the DtCyber operator interface.
   *
   * Arguments:
   *   command - the command to send. End-of-line is appended automatically.
   *   strmId  - identifier of the DtCyber stream. If omitted, the default
   *             is "dtCyber".
   */
  send(command, strmId) {
    const mgr = this.getStreamMgr(strmId);
    mgr.write(`${command}\n`);
  }

  /*
   * shutdown
   *
   * Execute a command sequence to shutdown DtCyber gracefully.
   *
   * Arguments:
   *   isExitAfterShutdown - true if the current process should exit after DtCyber has
   *                         been shutdown. If omitted, the default is true.
   *
   * Returns:
   *   A promise that is resolved when the shutdown is complete.
   */
  shutdown(isExitAfterShutdown) {
    const me = this;
    this.isExitAfterShutdown = (typeof isExitAfterShutdown === "undefined") ? true : isExitAfterShutdown;
    let promise = me.say("Starting shutdown sequence ...")
    .then(() => me.dsd("[UNLOCK."))
    .then(() => me.dsd("CHE"))
    .then(() => me.sleep(15000))
    .then(() => me.dsd("STEP."))
    .then(() => me.sleep(2000))
    .then(() => me.send("shutdown"));
    if (this.isExitAfterShutdown === false && typeof me.dtCyberChild !== "undefined") {
      promise = promise
      .then(() => new Promise((resolve, reject) => {
        me.dtCyberChild.on("exit", (code, signal) => {
          if (signal == null && code === 0) {
            resolve();
          }
        });
      }));
    }
    return promise;
  }

  /*
   * sleep
   *
   * Sleep for a specified number of milliseconds.
   *
   * Arguments:
   *   ms - the number of milliseconds to sleep
   *
   * Returns:
   *   A promise that is reolved when the specified number of milliseconds has elapsed.
   */
  sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /*
   * start
   *
   * Start DtCyber as a non-detached process and create a stream named "dtCyber" to manage it.
   *
   * Arguments:
   *   command - pathname of DtCyber executable
   *   args    - optional command line arguments to provide to DtCyber (e.g., "manual")
   *
   * Returns:
   *   A promise that is resolved when DtCyber has started and its operator interface is
   *   ready to accept commands.
   */
  start(command, args) {
    const me = this;
    this.streamMgrs.dtCyber = new DtCyberStreamMgr();
    return new Promise((resolve, reject) => {
      me.dtCyberChild = child_process.spawn(command, args, {
        stdio:["pipe", "pipe", process.stderr]
      });
      me.streamMgrs.dtCyber.setOutputStream(me.dtCyberChild.stdin);
      me.dtCyberChild.on("spawn", () => {
        resolve(me.dtCyberChild);
      });
      me.dtCyberChild.on("exit", (code, signal) => {
        const d = new Date();
        if (signal !== null) {
          console.log(`${d.toLocaleTimeString()} DtCyber exited due to signal ${signal}`);
          process.exit(1);
        }
        else if (code === 0) {
          console.log(`${d.toLocaleTimeString()} DtCyber exited normally`);
          if (typeof me.isExitAfterShutdown === "undefined" || me.isExitAfterShutdown === true) {
            process.exit(0);
          }
        }
        else {
          console.log(`${d.toLocaleTimeString()} DtCyber exited with status ${code}`);
          process.exit(code);
        }
      });
      me.dtCyberChild.on("error", err => {
        reject(err);
      });
      me.dtCyberChild.stdout.on("data", (data) => {
        me.streamMgrs.dtCyber.appendData(data);
      });
    });
  }

  /*
   * unmount
   *
   * Unmount a tape image from a specified tape drive.
   *
   * Arguments:
   *   ch   - channel number of the channel to which the tape drive is attached
   *   eq   - equipment number of the tape drive on the channel
   *   unit - unit number of the tape drive
   *
   * Returns:
   *   A promise that is resolved when the tape has been unmounted.
   */
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

  /*
   * unzip
   *
   * Extract all of the files from a ZIP archive.
   *
   * Arguments:
   *   srcPath - pathname of ZIP archive
   *   dstDir  - pathname of directory into which to extract files from the arvhive
   *
   * Returns:
   *   A promise that is resolved when the extraction is complete.
   */
  unzip(srcPath, dstDir) {
    return extract(srcPath, { dir: fs.realpathSync(dstDir) });
  }

  /*
   * waitJob
   *
   * Wait for a job to complete. Watch the printer stream for indications that a
   * specified job has completed successfully or failed. Jobs to be watched must
   * issue one these dayfile messages before they complete:
   *
   *    *** jobname COMPLETE
   *    *** jobname FAILED
   *
   * Normally, "jobname" is the jobname as specified by the job's job card.
   *
   * Arguments:
   *   jobName - jobname of the job to watch (normally the name specified on the
   *             job card)
   *
   * Returns:
   *   A promise that is resolved when the job completes successfully (i.e., when
   *   it issues the dayfile message "*** jobName COMPLETE").
   */
  waitJob(jobName) {
    return this.expect([
      {re:new RegExp(`\\*\\*\\* ${jobName} FAILED`, "i"), fn:new Error(`${jobName} failed`)},
      {re:new RegExp(`\\*\\*\\* ${jobName} COMPLETE`, "i")}
    ], "printer");
  }

  /*
   * wget
   *
   * Get a file from the web and store it in a specified directory. If the file
   * already exists in the directory, and it is less than 24 hours old, avoid
   * getting it from the web.
   *
   * Arguments:
   *   url      - URL of the file to get
   *   cacheDir - pathname of the directory in which to store the file
   *   filename - filename under which to store the file. If omitted, the
   *              filename is derived from the URL.
   *
   * Returns:
   *   A promise that is resolved when the file has been retrieved, if
   *   necessary, and is available for use.
   */
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
