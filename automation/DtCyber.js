const bz2           = require("unbzip2-stream");
const child_process = require("child_process");
const extract       = require("extract-zip");
const fs            = require("fs");
const ftp           = require("ftp");
const http          = require("http");
const https         = require("https");
const net           = require("net");

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
    fs.unwatchFile(this.printerFile);
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
      this.watcher = fs.watchFile(me.printerFile, {persistent:false, interval:250}, (current, previous) => {
        if (current.size > me.position) {
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
   * asciiToCdc
   *
   * Translate ASCII to CDC 6/12 display code.
   *
   * Arguments:
   *   ascii - the ASCII to convert
   *
   * Returns:
   *   CDC 6/12 display code
   */
  asciiToCdc(ascii) {
    let result = "";
    let i = 0;
  
    while (i < ascii.length) {
      let c = ascii.charAt(i++);
      switch (c) {
      case "a": result += "^A"; break;
      case "b": result += "^B"; break;
      case "c": result += "^C"; break;
      case "d": result += "^D"; break;
      case "e": result += "^E"; break;
      case "f": result += "^F"; break;
      case "g": result += "^G"; break;
      case "h": result += "^H"; break;
      case "i": result += "^I"; break;
      case "j": result += "^J"; break;
      case "k": result += "^K"; break;
      case "l": result += "^L"; break;
      case "m": result += "^M"; break;
      case "n": result += "^N"; break;
      case "o": result += "^O"; break;
      case "p": result += "^P"; break;
      case "q": result += "^Q"; break;
      case "r": result += "^R"; break;
      case "s": result += "^S"; break;
      case "t": result += "^T"; break;
      case "u": result += "^U"; break;
      case "v": result += "^V"; break;
      case "w": result += "^W"; break;
      case "x": result += "^X"; break;
      case "y": result += "^Y"; break;
      case "z": result += "^Z"; break;
      case "{": result += "^0"; break;
      case "|": result += "^1"; break;
      case "}": result += "^2"; break;
      case "~": result += "^3"; break;
      case "@": result += "@A"; break;
      case "^": result += "@B"; break;
      case ":": result += "@D"; break;
      case "`": result += "@G"; break;
      default:  result += c   ; break;
      }
    }
    return result;
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
   * cdcToAscii
   *
   * Translate CDC 6/12 display code to ASCII.
   *
   * Arguments:
   *   displayCode - the display code to convert
   *
   * Returns:
   *   ASCII result
   */
  cdcToAscii(displayCode) {
    let result = "";
    let i = 0;
  
    while (i < displayCode.length) {
      let c = displayCode.charAt(i++);
      if (c === "^" && i < displayCode.length) {
        c = displayCode.charAt(i++);
        switch (c) {
        case "A": result += "a"; break;
        case "B": result += "b"; break;
        case "C": result += "c"; break;
        case "D": result += "d"; break;
        case "E": result += "e"; break;
        case "F": result += "f"; break;
        case "G": result += "g"; break;
        case "H": result += "h"; break;
        case "I": result += "i"; break;
        case "J": result += "j"; break;
        case "K": result += "k"; break;
        case "L": result += "l"; break;
        case "M": result += "m"; break;
        case "N": result += "n"; break;
        case "O": result += "o"; break;
        case "P": result += "p"; break;
        case "Q": result += "q"; break;
        case "R": result += "r"; break;
        case "S": result += "s"; break;
        case "T": result += "t"; break;
        case "U": result += "u"; break;
        case "V": result += "v"; break;
        case "W": result += "w"; break;
        case "X": result += "x"; break;
        case "Y": result += "y"; break;
        case "Z": result += "z"; break;
        case "0": result += "{"; break;
        case "1": result += "|"; break;
        case "2": result += "}"; break;
        case "3": result += "~"; break;
        default:
          result += "^";
          i -= 1;
          break;
        }
      }
      else if (c === "@" && i < displayCode.length) {
        c = displayCode.charAt(i++);
        switch (c) {
        case "A": result += "@"; break;
        case "B": result += "^"; break;
        case "D": result += ":"; break;
        case "G": result += "`"; break;
        default:
          result += "@";
          i -= 1;
          break;
        }
      }
      else {
        result += c;
      }
    }
    return result;
  }

  /*
   * connect
   *
   * Create a connection to the DtCyber operator interface. The port on which to connect
   * may be specified as a parameter. If the port is not specified, the method looks for
   * a cyber.ini in the current working directory or its parent, parses the file for a
   * set_operator_port command, and attempts to connect to the port number defined by
   * that command. Creates an instance of DtCyberStreamMgr to manage the connection that
   * is established.
   *
   * Arguments:
   *   port - optional port number
   *
   * Returns:
   *   A promise that is resolved when the connection has been established
   */
  connect(port) {
    const me = this;
    if (typeof me.isConnected !== "undefined" && me.isConnected) {
      return Promise.resolve();
    }
    const props = this.getIniProperties();
    let   host  = null;
    if (typeof port !== "undefined") {
      const ci = port.indexOf(":");
      if (ci !== -1) {
        host = port.substring(0, ci).trim();
        port = parseInt(port.substring(ci + 1));
      }
      else if (port.indexOf(".") !== -1) {
        host = port;
        port = undefined;
      }
    }
    if (host === null) {
      host = "127.0.0.1";
      if (typeof props["cyber"] !== "undefined") {
        for (const line of props["cyber"]) {
          let ei = line.indexOf("=");
          if (ei === -1) continue;
          let key = line.substring(0, ei).trim().toUpperCase();
          if (key === "IPADDRESS") {
            host = line.substring(ei + 1).trim();
          }
        }
      }
    }
    if (typeof port === "undefined") {
      if (typeof this.operatorPort === "undefined" && typeof props["operator.nos287"] !== "undefined") {
        for (const line of props["operator.nos287"]) {
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
      port = this.operatorPort;
    }
    this.streamMgrs.dtCyber = new DtCyberStreamMgr();
    return new Promise((resolve, reject) => {
      me.isConnected = false;
      me.isExitOnClose = true;
      const doConnect = (deadline, callback) => {
        if (me.isConnected) {
          callback(null);
          return;
        }
        try {
          me.socket = net.createConnection({port:port, host:host}, () => {
            me.isConnected = true;
            callback(null);
          });
        }
        catch (err) {
          if (Date.now() > deadline) {
            callback(err);
          }
          else {
            setTimeout(() => {
              doConnect(deadline, callback);
            }, 500);
          }
          return;
        }
        me.streamMgrs.dtCyber.setOutputStream(me.socket);
        me.socket.on("data", data => {
          me.streamMgrs.dtCyber.appendData(data);
        });
        me.socket.on("close", () => {
          if (me.isConnected && me.isExitOnClose) {
            console.log(`${new Date().toLocaleTimeString()} DtCyber disconnected`);
            process.exit(0);
          }
          me.isConnected = false;
          if (typeof me.socketCloseCallback === "function") {
            me.socketCloseCallback();
            me.socketCloseCallback = undefined;
          }
        });
        me.socket.on("error", err => {
          if (me.isConnected) {
            console.log(`${new Date().toLocaleTimeString()} ${err}`);
          }
          else if (Date.now() > deadline) {
            callback(err);
          }
          else {
            setTimeout(() => {
              doConnect(deadline, callback);
            }, 500);
          }
        });
      };
      doConnect(Date.now() + 10000, err => {
        if (err === null) {
          resolve();
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
   * createJobWithOutput
   *
   * Load a card deck on a specified card reader, wait for the associated
   * job to complete, and return its output.
   *
   * Note that this method creates a temporary file named "$$$.job" in the
   * current working directory. This file contains the full card deck that
   * is loaded on the card reader. It is deleted automatically when the job
   * completes successfully.
   *
   * Arguments:
   *   ch       - channel number of the channel to which the reader is attached
   *   eq       - equipment number of the reader on the channel
   *   body     - body of the job (not including job and USER commands, these
   *              are added automatically, based upon options)
   *   options  - optional object providing parameters such as
   *              job nqme, credentials, and job data
   *
   * Returns:
   *   A promise that is resolved when the job has completed.
   *   The value of the promise is the job output.
   */
  createJobWithOutput(ch, eq, body, options) {
    const me     = this;
    let jobname  = "JOB";
    let username = "INSTALL";
    let password = "INSTALL";
    let data     = null;
    if (typeof options === "object") {
      if (typeof options.jobname  === "string") jobname  = options.jobname;
      if (typeof options.username === "string") username = options.username;
      if (typeof options.user     === "string") username = options.user;
      if (typeof options.password === "string") password = options.password;
      if (typeof options.data     === "string") data     = options.data;
    }
    const beginOutput = ` *** ${jobname} BEGIN OUTPUT ***`;
    const endOutput   = ` *** ${jobname} END OUTPUT ***`;
    if (Array.isArray(body)) body = body.join("\n") + "\n";
    let job = `${jobname}.\n$USER,${username},${password}.\n`
      + `$NOTE,OUTPUT,NR./${beginOutput}\n`
      + body
      + `$NOTE,OUTPUT,NR./${endOutput}\n`
      + `$PACK,OUTPUT.\n*** ${jobname} COMPLETE\n`
      + "EXIT.\n$NOEXIT.\n$SKIPEI,OUTPUT.\n"
      + `$NOTE,OUTPUT,NR./${endOutput}\n`
      + `$PACK,OUTPUT.\n*** ${jobname} FAILED\n`;
    if (data !== null) job += `~eor\n${data}`;
    fs.writeFileSync("decks/$$$.job", job);
    return new Promise((resolve, reject) => {
      let output = "";
      me.runJob(ch, eq, "decks/$$$.job", data => {
        output += data;
      })
      .then(() => {
        fs.unlinkSync("decks/$$$.job");
        let bi = output.indexOf(beginOutput);
        if (bi >= 0) {
          output = output.substring(bi + beginOutput.length);
          bi = output.indexOf("\n");
          output = output.substring(bi + 1);
        }
        let ei = output.indexOf(endOutput);
        if (ei >= 0) {
          output = output.substring(0, ei);
        }
        let lines = output.split("\n");
        lines = lines.slice(0, lines.length - 1);
        let result = "";
        for (const line of lines) {
          result += line.substring(1) + "\n";
        }
        resolve(result);
      });
    });
  }

  /*
   * dis
   *
   * Call DIS, provide it with a sequence of commands to execute, and then drop DIS. 
   * If a job name is provided, commands are entered using the DIS ELS command.
   *
   * Arguments:
   *   commands - a string representing a single command to execute, or
   *              an array of strings representing a sequence of commands
   *              to execute. 
   *   jobName  - optional job name. If provided, ELS is called to collect
   *              commands, and waitJob() is used to wait for the collected
   *              sequence to complete (or fail).
   *   ui       - optional user index under which to execute the sequence of
   *              commands. If omitted, user index 377777 is used.
   *
   * Returns:
   *   A promise that is resolved when the sequence of commands has completed and/or
   *   DIS has dropped
   */
  dis(commands, jobName, ui) {
    const me = this;
    let hasJobName = false;
    let hasUi = false;
    if (typeof jobName === "string") {
      hasJobName = true;
      if (typeof ui === "number") {
        hasUi = true;
      }
    }
    else if (typeof jobName === "number") {
      ui = jobName;
      hasUi = true;
    }
    let list = [`SUI,${hasUi ? ui.toString() : "377777"}.`];
    if (!Array.isArray(commands)) {
      commands = [commands];
    }
    if (hasJobName) {
      list.push(`#2000#ELS.NOTE./1${jobName}`);
      list = list.concat(commands).concat([
        `*** ${jobName} COMPLETE`,
        "EXIT.",
        `*** ${jobName} FAILED`,
        "[!",
        ".!"
      ]);
    }
    else {
      list.push(`#2000#${commands.shift()}`);
      list = list.concat(commands);
    }
    list.push("#1000#[DROP.");
    let promise = this.dsd("[X.DIS.")
    .then(() => me.sleep(2000))
    .then(() => me.dsd(list));
    if (hasJobName) {
      promise = promise
      .then(() => me.waitJob(jobName));
    }
    else {
      promise = promise
      .then(() => me.sleep(1000));
    }
    return promise;
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
    return new Promise((resolve, reject) => {
      me.isExitOnClose = false;
      if (typeof me.socket !== "undefined"
          && typeof me.socket.destroyed !== "undefined"
          && me.socket.destroyed === false) {
        me.socketCloseCallback = () => {
          me.socket.destroy();
          resolve();
        };
        me.socket.end();
      }
      else {
        resolve();
      }
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
   * engageOperator
   *
   * Begin serving as an intermediary between the user and the DtCyber
   * operator interface.
   *
   * Arguments:
   *   commands - optional array of commands to recognize. Each element
   *              of the array is an object containing these properties:
   *              names: array of names and aliases of the command
   *              fn:    function implementing the command. Two parameters
   *                     are passed to the function: a reference to this
   *                     DtCyber object instance, and the remainder of the
   *                     command line following the command token itself.
   *                     The function is expected to return a promise that
   *                     is resolved when the command is complete.
   *              desc:  description of the command
   *
   * Returns:
   *   A promise that is resolved when stdin is closed.
   */
  engageOperator(commands) {
    const me = this;
    if (typeof commands === "undefined") {
      commands = [];
    }
    return new Promise((resolve, reject) => {
      let mgr = me.getStreamMgr();
      let str = "";
      mgr.startConsumer(data => {
        process.stdout.write(data);
        return true;
      });
      process.stdin.on("data", data => {
        str += data
        while (true) {
          let eoli = str.indexOf("\n");
          if (eoli === -1) break;
          let line = str.substring(0, eoli).trim();
          str = str.substring(eoli + 1);
          let si = line.indexOf(" ");
          let cmdToken = line;
          let rest = "";
          if (si !== -1) {
            cmdToken = line.substring(0, si);
            rest = line.substring(si + 1).trim();
          }
          cmdToken = cmdToken.toLowerCase();
          let cmdDefn = null;
          for (const defn of commands) {
            if (defn.names.indexOf(cmdToken) !== -1) {
              cmdDefn = defn;
              break;
            }
          }
          if (cmdDefn !== null) {
            cmdDefn.fn(me, rest)
            .then(() => {
              mgr = me.getStreamMgr();
              mgr.startConsumer(data => {
                process.stdout.write(data);
                return true;
              });
              mgr.write("\n");
            });
            break;
          }
          else if (cmdToken === "exit") {
            resolve();
            break;
          }
          else if (cmdToken === "help" || cmdToken === "?") {
            if (rest === "") {
              process.stdout.write("---------------------------\n");
              process.stdout.write("List of extended commands:\n");
              process.stdout.write("---------------------------\n\n");
              for (const defn of commands) {
                for (const name of defn.names) {
                  process.stdout.write(`    > ${name}\n`);
                }
              }
            }
            else {
              let cmdDefn = null;
              for (const defn of commands) {
                if (defn.names.indexOf(rest) !== -1) {
                  cmdDefn = defn;
                  break;
                }
              }
              if (cmdDefn !== null) {
                process.stdout.write(`\n    > ${cmdDefn.desc}\n`);
                mgr.write("\n");
                break;
              }
            }
          }
          else if (cmdToken === "set_operator_port" || cmdToken === "sop") {
            process.stdout.write("Command ignored; cannot change operator port while connected to it\n");
            break;
          }
          mgr.write(`${line}\n`);
        }
      });
      process.stdin.on("close", () => {
        resolve();
      });
      mgr.write("\n");
    });
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
        stdio:[0, 1, 2]
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
   *   observer - optional callback to which all data received will
   *              be sent.
   *
   * Returns:
   *   A promise that is resolved when a match is indicated.
   */
  expect(patterns, strmId, observer) {
    const me = this;
    if (typeof strmid === "function" && typeof observer === "undefined") {
      observer = strmid;
      strmid = undefined;
    }
    return new Promise((resolve, reject) => {
      let str = "";
      let mgr = me.getStreamMgr(strmId);
      mgr.startConsumer(data => {
        if (typeof observer === "function") observer(data);
        if (str.length > 8192) {
          str = str.substring(str.length - 8192);
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
   * findDtCyber
   *
   * Find the DtCyber executable.
   *
   * Returns:
   *   Pathname of DtCyber executable, or null if not found
   */
  findDtCyber() {
    const paths = process.platform === "win32"
      ? ["../x64/Release/DtCyber/DtCyber.exe", "../Win32/Release/DtCyber/DtCyber.exe",
         "../x64/Debug/DtCyber/DtCyber.exe", "../Win32/Debug/DtCyber/DtCyber.exe",
         "./dtcyber.exe", "../dtcyber.exe"]
      : ["./dtcyber", "../dtcyber", "../bin/dtcyber"];
    for (const path of paths) {
      if (fs.existsSync(path)) return path;
    }
    return null;
  }

  /*
   * flushCache
   *
   * Flush all cached data to force re-reading/re-calculating.
   */
  flushCache() {
    this.operatorPort = undefined;
    this.iniProperties = undefined;
  }

  /*
   * getIniProperties
   *
   * Open and read the cyber.ini and cyber.ovl files and return
   * an object reflecting the merged set of properties.
   *
   * Returns:
   *   The merged properties.
   */
  getIniProperties() {
    if (typeof this.iniProperties === "undefined") {
      this.iniProperties = {};
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
      this.readPropertyFile(path, this.iniProperties);
      path = path.substring(0, path.lastIndexOf(".")) + ".ovl";
      if (fs.existsSync(path)) {
        this.readPropertyFile(path, this.iniProperties);
      }
    }
    return this.iniProperties;
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
   * postJob
   *
   * Submit a job to the local mainframe using the NOS 2 HTTP server and
   * wait for the associated job to complete. See description of waitJob
   * for information about the expected format of the card deck.
   *
   * Arguments:
   *   job      - string or array representing the job
   *   hostname - optional hostname or IP address of the HTTP server (default: localhost)
   *
   * Returns:
   *   A promise that is resolved when the job is complete. The job's
   *   output is provided as the promise's result value.
   */
  postJob(job, hostname) {
    if (Array.isArray(job)) {
      job = job.join("\n") + "\n";
    }
    if (typeof hostname === "undefined") hostname = "127.0.0.1";
    let match = /^([^,.(])/.exec(job);
    const jobName = match[1];
    const options = {
      hostname: hostname,
      port: 80,
      path: "/",
      method: "POST",
      headers: {
        "Content-Type": "application/vnd.cdc-job",
        "Content-Length": job.length
      }
    };
    return new Promise((resolve, reject) => {
      let text = "";
      const req = http.request(options, res => {
        res.setEncoding("utf8");
        res.on("data", chunk => {
          text += chunk;
        });
        res.on("end", () => {
          text = text.replaceAll("\r\n", "\n");
          let dayfile,output;
          const eor = text.lastIndexOf("~eor\n");
          if (eor >= 0) {
            output  = text.substring(0, eor);
            dayfile = text.substring(eor + 5);
          }
          else {
            output  = "";
            dayfile = output;
          }
          if (dayfile.indexOf(`*** ${jobName} FAILED`) >= 0) {
            reject(new Error(`${jobName} failed`));
          }
          else {
            resolve(output);
          }
        });
      });
      req.on("error", err => {
        reject(err);
      });
      req.write(job);
      req.end();
    });
  }

  /*
   * putFile
   *
   * Use FTP to upload a file to a permanent file catalog on NOS 2.
   *
   * Arguments:
   *   name     - the file name to create on NOS 2
   *   text     - string or array representing the content of the file
   *   options  - optional object providing FTP parameters, e.g., user, password,
   *              host, and port
   *
   * Returns:
   *   A promise that is resolved when the transfer is complete.
   */
  putFile(name, text, options) {
    const me = this;
    const retryDelay = 5;
    const maxRetries = 10;
    if (typeof options === "undefined") {
      options = {
        user: "INSTALL",
        password: "INSTALL",
        host: "127.0.0.1",
        port: 21
      }
    }
    if (typeof options.host === "undefined") {
      options.host = "127.0.0.1"
    }
    if (typeof options.username !== "undefined"
        && typeof options.user === "undefined") {
      options.user = options.username;
    }
    if (Array.isArray(text)) {
      text = text.join("\r\n") + "\r\n";
    }
    else {
      if (text.endsWith("\n")) text = text.substring(0, text.length - 1);
      const lines = text.split("\n");
      text = "";
      for (const line of lines) {
        text += line.endsWith("\r") ? line + "\n" : line + "\r\n";
      }
    }
    const sender = (tryNo, callback) => {
      const retry = err => {
        if (tryNo <= maxRetries) {
          console.log(`${new Date().toLocaleTimeString()} FTP attempt ${tryNo} of ${maxRetries} for ${name} failed`);
          console.log(`${new Date().toLocaleTimeString()}   retrying after ${retryDelay} seconds ...`);
          setTimeout(() => {
            sender(tryNo + 1, callback);
          }, retryDelay * 1000);
        }
        else {
          callback(err);
        }
      };
      const client = new ftp();
      client.on("ready", () => {
        client.ascii(err => {
          if (err) {
            retry(err);
          }
          else {
            client.put(Buffer.from(text), name, err => {
              if (err) {
                retry(err);
              }
              else {
                client.logout(err => {
                  client.end();
                  callback();
                });
              }
            });
          }
        });
      });
      client.on("error", err => {
        retry(err);
      });
      client.connect(options);
    };
    return new Promise((resolve, reject) => {
      sender(1, err => {
        if (err) {
          reject(err);
        }
        else {
          resolve();
        }
      });
    });
  }

  /*
   * readPropertyFile
   *
   * Read and parse a property file containing customized configuration
   * property definitions. The file may contain multiple sections, each
   * headed by a name, as in:
   *
   *    [sectionname]
   *
   * Arguments:
   *   pathname  - optional pathname of property file, default is "site.cfg"
   *   props     - object that will be updated with parsed definitions
   */
  readPropertyFile(pathname, props) {
    if (typeof pathname !== "string") {
      if (fs.existsSync("site.cfg")) {
        props = pathname;
        pathname = "site.cfg";
      }
      else {
        return;
      }
    }
    const lines = fs.readFileSync(pathname, "utf8");
    let sectionKey = "";

    for (let line of lines.split("\n")) {
      line = line.trim();
      if (line.length < 1 || /^[;#*]/.test(line)) continue;
      if (line.startsWith("[")) {
        let bi = line.indexOf("]");
        if (bi < 0) {
          throw new Error(`Invalid section key: \"${line}\"`);
        }
        sectionKey = line.substring(1, bi).trim();
      }
      else if (sectionKey !== "") {
        if (typeof props[sectionKey] === "undefined") {
          props[sectionKey] = [];
        }
        props[sectionKey].push(line);
      }
      else {
        throw new Error(`Property defined before first section key: \"${line}\"`);
      }
    }
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
   *   observer  - optional callback to which all data received will be sent
   *
   * Returns:
   *   A promise that is resolved when the job has completed.
   */
  runJob(ch, eq, deck, jobParams, observer) {
    const me = this;
    if (typeof jobParams === "function" && typeof observer === "undefined") {
      observer = jobParams;
      jobParams = undefined;
    }
    return new Promise((resolve, reject) => {
      try {
        const lines = fs.readFileSync(deck, {encoding:"utf8"}).split("\n");
        let jobName = null;
        for (let line of lines) {
          if (line.match(/^[A-Z0-9$]+/)) {
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
          .then(() => me.waitJob(jobName, observer))
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
   * setExitOnClose
   *
   * Sets the indicator that determines whether the caller will exit automatically
   * when DtCyber closes its operator command connection. Normally, DtCyber closes
   * the connection only when it exits.
   *
   * Arguments:
   *   isExitOnClose - true if the caller should exit on close, false otherwise
   */
  setExitOnClose(isExitOnClose) {
    this.isExitOnClose = isExitOnClose;
  }

  /*
   * shutdown
   *
   * Execute a command sequence to shutdown DtCyber gracefully.
   *
   * Arguments:
   *   isExitAfterShutdown - true if the current process should exit after DtCyber has
   *                         been shut down. If omitted, the default is true.
   *
   * Returns:
   *   A promise that is resolved when the shutdown is complete.
   */
  shutdown(isExitAfterShutdown) {
    const me = this;
    if (typeof isExitAfterShutdown === "undefined") {
      isExitAfterShutdown = true;
    }
    return me.say("Starting shutdown sequence ...")
    .then(() => me.dsd("[UNLOCK."))
    .then(() => me.dsd("CHECK#2000#"))
    .then(() => me.sleep(20000))
    .then(() => me.dsd("STEP."))
    .then(() => me.sleep(2000))
    .then(() => {
      me.isExitOnClose = isExitAfterShutdown;
      return Promise.resolve();
    })
    .then(() => me.send("shutdown"))
    .then(() => me.expect([{ re: /Goodbye for now/ }]))
    .then(() => me.say("Shutdown complete"))
    .then(() => {
      if (isExitAfterShutdown || typeof me.dtCyberChild === "undefined" || me.dtCyberChild.exitCode !== null) {
        return Promise.resolve();
      }
      else {
        return new Promise((resolve, reject) => {
          me.shutdownResolver = () => {
            resolve();
          };
        });
      }
    });
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
   *   args    - optional command line arguments to provide to DtCyber (e.g., ["manual"])
   *   options - optional options for child_process.spawn()
   *
   * Returns:
   *   A promise that is resolved when DtCyber has started and its operator interface is
   *   ready to accept commands.
   */
  start(args, options) {
    const me = this;
    const command = this.findDtCyber();
    if (command === null) {
      return Promise.reject(new Error("DtCyber executable not found"))
    }
    if (typeof args !== "undefined") {
      if (typeof args === "string") {
        args = [args];
      }
      else if (!Array.isArray(args)) {
        if (typeof options === "undefined") {
          options = args;
          args = [];
        }
      }
    }
    if (typeof options === "undefined") {
      options = {
        stdio:["pipe", "pipe", process.stderr]
      };
    }
    this.streamMgrs.dtCyber = new DtCyberStreamMgr();
    return new Promise((resolve, reject) => {
      me.dtCyberChild = child_process.spawn(command, args, options);
      if (options.detached) {
        if (typeof options.unref === "undefined" || options.unref === true) {
          me.dtCyberChild.unref();
        }
        resolve();
        me.dtCyberChild.on("exit", (code, signal) => {
          const d = new Date();
          if (signal !== null) {
            console.log(`${d.toLocaleTimeString()} DtCyber exited due to signal ${signal}`);
          }
          else if (code !== 0) {
            console.log(`${d.toLocaleTimeString()} DtCyber exited with status ${code}`);
          }
          else {
            console.log(`${d.toLocaleTimeString()} DtCyber exited normally`);
          }
          if (typeof me.shutdownResolver === "function") {
            me.shutdownResolver();
            delete me.shutdownResolver;
          }
          delete me.dtCyberChild;
        });
      }
      else {
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
            if (typeof me.shutdownResolver === "function") {
              me.shutdownResolver();
              delete me.shutdownResolver;
              delete me.dtCyberChild;
            }
            else {
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
      }
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
   *   jobName  - jobname of the job to watch (normally the name specified on the
   *              job card)
   *   observer - optional callback to which all data received will be sent
   *
   * Returns:
   *   A promise that is resolved when the job completes successfully (i.e., when
   *   it issues the dayfile message "*** jobName COMPLETE").
   */
  waitJob(jobName, observer) {
    return this.expect([
      {re:new RegExp(`\\*\\*\\* ${jobName} FAILED`, "i"), fn:new Error(`${jobName} failed`)},
      {re:new RegExp(`\\*\\*\\* ${jobName} COMPLETE`, "i")}
    ], "printer", observer);
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
   *   progress - optional callback function to which to report progress
   *
   * Returns:
   *   A promise that is resolved when the file has been retrieved, if
   *   necessary, and is available for use.
   */
  wget(url, cacheDir, filename, progress) {
    const me = this;
    if (typeof filename === "function") {
      progress = filename;
    }
    const pathname = new URL(url).pathname;
    if (typeof filename === "undefined" || typeof filename === "function") {
      const li = pathname.lastIndexOf("/");
      filename = pathname.substring((li >= 0) ? li + 1 : 0);
    }
    const cachePath = `${cacheDir}/${filename}`;
    if (fs.existsSync(cachePath)) {
      const stat = fs.statSync(cachePath);
      if (Date.now() - stat.ctimeMs < (24 * 60 * 60 * 1000)) {
        if (typeof progress === "function") {
          progress(stat.size, stat.size);
        }
        return Promise.resolve();
      }
    }
    return new Promise((resolve, reject) => {
      if (fs.existsSync(cachePath)) {
        fs.unlinkSync(cachePath);
      }
      const strm = fs.createWriteStream(cachePath, { mode: 0o644 });
      const svc = url.startsWith("https:") ? https : http;
      svc.get(url, res => {
        let contentLength = -1;
        if (typeof res.headers["content-length"] !== "undefined") {
          contentLength = parseInt(res.headers["content-length"]);
        }
        let byteCount = 0;
        res.on("data", data => {
          strm.write(data);
          byteCount += data.length;
          if (typeof progress === "function") {
            progress(byteCount, contentLength);
          }
        });
        res.on("end", () => {
          strm.end(() => {
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
              me.wget(location, cacheDir, filename, progress)
              .then(path => { resolve(path); })
              .catch(err => { reject(err); });
              break;
            default:
              reject(new Error(`${res.statusCode}: failed to get ${url}`));
              break;
            }
          });
        });
      }).on("error", err => {
        reject(err);
      });
    });
  }
}

module.exports = DtCyber;
