const net = require("net");

const ATerm        = require("../webterm/www/js/aterm");
const Machine      = require("../webterm/www/js/machine-comm");
const PTerm        = require("../webterm/www/js/pterm");
const PTermClassic = require("../webterm/www/js/pterm-classic");

/*
 * RenderingContext
 *
 * This class implements the interface defined by the 2D rendering context of the
 * HTML canvas element. It enables capturing graphics actions executed by a browser-based
 * terminal emulator.
 */
class RenderingContext {

  constructor(streamMgr) {
    this.streamMgr = streamMgr;
    this.x = 0;
    this.y = 0;
  }

  beginPath() {
  }

  clearRect(x, y, w, h) {
    if (typeof this.imageData === "undefined") return;
    if (x + w > this.imageData.width ) w = this.imageData.width  - x;
    if (y + h > this.imageData.height) h = this.imageData.height - y;
    for (let r = 0; r < h; r++) {
      let i = (x + y * this.imageData.width) * 4;
      for (let k = 0; k < w; k++) {
         this.imageData.data[i++] = 0 // R
         this.imageData.data[i++] = 0 // G
         this.imageData.data[i++] = 0 // B
         this.imageData.data[i++] = 0 // A
      }
      y += 1;
    }
  }

  createImageData(w, h) {
    let data = new Uint8Array(w * h * 4);
    data.fill(0);
    this.imageData = {
      width: w,
      height: h,
      data: data
    };
  }

  fillRect(x, y, w, h) {
    if (typeof this.imageData === "undefined") return;
    if (x + w > this.imageData.width ) w = this.imageData.width  - x;
    if (y + h > this.imageData.height) h = this.imageData.height - y;
    for (let r = 0; r < h; r++) {
      let i = (x + y * this.imageData.width) * 4;
      for (let k = 0; k < w; k++) {
         this.imageData.data[i++] = 0xff // R
         this.imageData.data[i++] = 0xff // G
         this.imageData.data[i++] = 0xff // B
         this.imageData.data[i++] = 0xff // A
      }
      y += 1;
    }
  }

  fillText(s, x, y) {
    if (typeof this.isGraphical === "undefined" || this.isGraphical === false) {
      if (x === 0 && this.x !== 0) this.streamMgr.appendText("\r");
      if (y > this.y) this.streamMgr.appendText("\n");
    }
    this.x = x;
    this.y = y;
    this.streamMgr.appendText(s);
    for (let i = 0; i < s.length; i++) {
      this.fillRect(x, y, this.fontWidth, this.fontHeight);
      x += this.fontWidth;
    }
  }

  getImageData(x, y, w, h) {
    if (typeof this.imageData === "undefined") return [];
    if (x + w > this.imageData.width ) w = this.imageData.width  - x;
    if (y + h > this.imageData.height) h = this.imageData.height - y;
    let data = new Uint8Array(w * h * 4);
    let result = {
      width: w,
      height: h,
      data: data
    }
    let d = 0;
    while (h-- > 0) {
      let i = (x + y * this.imageData.width) * 4;
      for (let k = 0; k < w; k++) {
         data[d++] = this.imageData.data[i++]; // R
         data[d++] = this.imageData.data[i++]; // G
         data[d++] = this.imageData.data[i++]; // B
         data[d++] = this.imageData.data[i++]; // A
      }
      y += 1;
    }
    return result;
  }

  lineTo(x, y) {
    this.x = x;
    this.y = y;
  }

  moveTo(x, y) {
    this.x = x;
    this.y = y;
  }

  putImageData(data, x, y) {
    if (typeof this.imageData === "undefined") return;
    let w = data.width;
    let h = data.height;
    if (x + w > this.imageData.width ) w = this.imageData.width  - x;
    if (y + h > this.imageData.height) h = this.imageData.height - y;
    for (let r = 0; r < h; r++) {
      let d = r * data.width * 4;
      let i = (x + y * this.imageData.width) * 4;
      for (let k = 0; k < w; k++) {
         this.imageData.data[i++] = data.data[d++] // R
         this.imageData.data[i++] = data.data[d++] // G
         this.imageData.data[i++] = data.data[d++] // B
         this.imageData.data[i++] = data.data[d++] // A
      }
      y += 1;
    }
  }

  setFontHeight(fontHeight) {
    this.fontHeight = fontHeight;
    this.y = fontHeight;
  }

  setFontWidth(fontWidth) {
    this.fontWidth = fontWidth;
    this.x = 0;
  }

  stroke() {
  }
}

/*
 * StreamMgr
 *
 * This class manages the input and output streams associated with a
 * terminal connection to DtCyber.
 */
class StreamMgr {

  constructor() {
    this.receivedText = "";
  }

  appendText(text) {
    this.receivedText += text;
    if (typeof this.consumer === "function") {
      if (this.consumer(this.receivedText) === false) {
        //
        // When the consumer returns false, this indicates that
        // it has completed its mission and should no longer be
        // applied to received text.
        //
        this.endConsumer();
      }
      this.clearInputStream();
    }
  }

  clearInputStream() {
    this.receivedText = "";
  }

  endConsumer() {
    delete this.consumer;
  }

  setOutputStream(stream) {
    this.outputStream = stream;
  }

  startConsumer(consumer) {
    this.consumer = consumer;
    if (this.receivedText.length > 0) {
      if (consumer(this.receivedText) === false) {
        this.endConsumer();
      }
      this.clearInputStream();
    }
  }
}

/*
 * Terminal
 *
 * This is the base class of the module. It provides a collection of methods facilitating
 * interaction with a terminal emulator. Normally, subclasses specific to terminal types
 * are used instead of using this class directly.
 */
class BaseTerminal {

  constructor(emulator) {
    this.minimumKeyInterval = 0;
    this.isDebug = false;
    this.keyQueue = [];
    this.emulator = emulator;
    this.machine = new Machine(null);
    this.machine.setTerminal(this.emulator);
    this.machine.setReceivedDataHandler(data => {
      if (typeof this.tracer === "function") this.tracer("R", data);
      this.emulator.renderText(data);
    });
    this.machine.setSender(data => {
      if (typeof this.tracer === "function") this.tracer("S", data);
      this.socket.write(data);
    });
    this.streamMgr = new StreamMgr();
    let ctx = new RenderingContext(this.streamMgr);
    this.emulator.context = ctx;
    ctx.setFontHeight(this.emulator.fontHeight);
    ctx.setFontWidth(this.emulator.fontWidth);
    this.emulator.setUplineDataSender(data => {
      this.machine.sender(data);
    });
  }

  /*
   * connect
   *
   * Create an interactive terminal connection. Creates an instance of StreamMgr
   * to manage the connection that is established.
   *
   * Arguments:
   *   port - port number to which to connect
   *
   * Returns:
   *   A promise that is resolved when the connection has been established
   */
  connect(port) {

    const me = this;

    const doTelnet = data => {
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
        if (me.telnetCommand.length > 0) {
          me.telnetCommand.push(data[i]);
          if (me.telnetCommand.length > 2) {
            switch (me.telnetCommand[1]) {
            case 254: // DON'T
              me.telnetCommand[1] = 252; // WON'T
              if (me.telnetCommand[2] === 1) { // Echo
                me.telnetEcho = false;
              }
              me.socket.write(Buffer.from(me.telnetCommand));
              me.telnetCommand = [];
              break;
            case 253: // DO
              switch (me.telnetCommand[2]) {
              case 0:  // Binary
              case 3:  // SGA
              case 24: // Terminal Type
                me.telnetCommand[1] = 251; // WILL
                break;
              case 1:  // Echo
                me.telnetEcho = true;
                me.telnetCommand[1] = 251; // WILL
                break;
              default:
                me.telnetCommand[1] = 252; // WON'T
                break;
              }
              me.socket.write(Buffer.from(me.telnetCommand));
              me.telnetCommand = [];
              break;
            case 252: // WON'T
              me.telnetCommand[1] = 254; // DON'T
              me.socket.write(Buffer.from(me.telnetCommand));
              me.telnetCommand = [];
              break;
            case 251: // WILL
              if (me.telnetCommand[2] === 0       // Binary
                  || me.telnetCommand[2] === 1    // Echo
                  || me.telnetCommand[2] === 3) { // SGA
                me.telnetCommand[1] = 253; // DO
              }
              else {
                me.telnetCommand[1] = 254; // DON'T
              }
              me.socket.write(Buffer.from(me.telnetCommand));
              me.telnetCommand = [];
              break;
            case 250: // SB
              if (me.telnetCommand[me.telnetCommand.length - 1] === 240) { // if SE
                if (me.telnetCommand.length > 3
                    && me.telnetCommand[2] === 24    // Terminal Type
                    && me.telnetCommand[3] === 1) {  // Send
                  me.telnetCommand[3] = 0; // Is
                  me.socket.write(Buffer.from(me.telnetCommand.slice(0, 4)));
                  me.socket.write("XTERM");
                  me.telnetCommand[1] = 240; // SE
                  me.socket.write(Buffer.from(me.telnetCommand.slice(0, 2)));
                }
                me.telnetCommand = [];
              }
              break;
            default:
              me.log(`Unsupported Telnet protocol option: ${me.telnetCommand[1]}`);
              me.telnetCommand = [];
              break;
            }
            start = i + 1;
          }
          else if (me.telnetCommand.length > 1) {
            switch (me.telnetCommand[1]) {
            case 241: // NOP
            case 242: // Data Mark
            case 243: // Break
            case 244: // Interrupt Process
            case 245: // Abort Output
            case 246: // Are You There
            case 247: // Erase Character
            case 248: // Erase Line
            case 249: // Go Ahead
              me.telnetCommand = [];
              start = i + 1;
            case 250: // SB
            case 251: // WILL
            case 252: // WON'T
            case 253: // DO
            case 254: // DON'T
              // just collect the byte
              break;
            case 255: // escaped FF
              me.machine.receivedDataHandler("\xff");
              me.telnetCommand = [];
              start = i + 1;
              break;
            default:
              me.log(`Unsupported Telnet protocol option: ${me.telnetCommand[1]}`);
              me.telnetCommand = [];
              break;
            }
          }
        }
        else if (data[i] === 255) { // IAC
          me.telnetCommand.push(data[i]);
          if (i > start) {
            me.machine.receivedDataHandler(data.slice(start, i));
          }
        }
        else if (data[i] === 0x0d && i + 1 < data.length && data[i + 1] === 0x00) {
          me.machine.receivedDataHandler(data.slice(start, i + 1));
          start = i + 2;
          i += 1;
        }
        i += 1;
      }
      if (i > start && me.telnetCommand.length < 1) {
        me.machine.receivedDataHandler(data.slice(start, i));
      }
    };

    const doConnect = callback => {
      let host = "127.0.0.1";
      if (typeof port === "string" && port.indexOf(":") >= 0) {
        let ci = port.indexOf(":");
        host = port.substring(0, ci);
        port = port.substring(ci + 1);
        if (host.length < 1) host = "127.0.0.1";
      }
      me.socket = net.createConnection({port:port, host:host}, () => {
        me.isConnected = true;
        callback(null);
      });
      me.streamMgr.setOutputStream(me.socket);
      me.socket.on("data", data => {
        if (me.isTelnetConnection) {
          doTelnet(data);
        }
        else {
          me.machine.receivedDataHandler(data);
        }
      });
      me.socket.on("close", () => {
        if (me.isConnected && me.isExitOnClose) {
          me.log("host disconnected");
          process.exit(0);
        }
        me.isConnected = false;
      });
      me.socket.on("error", err => {
        if (me.isConnected) {
          me.log(err);
        }
        else if (Date.now() >= me.connectDeadline) {
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
      me.connectDeadline = Date.now() + 2000;
      me.isConnected = false;
      me.isExitOnClose = true;
      me.telnetCommand = [];
      doConnect(err => {
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
      me.socket.end(() => {
        me.socket.destroy();
        resolve();
      });
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
   *                    be applied to the stream, or a number specifying
   *                    the maximum number of seconds to wait for a match
   *                fn: if string "continue", other patterns in the array
   *                      continue to be applied. This provides a way to 
   *                      recognize an intermediate pattern in the stream
   *                      and continue looking for other patterns. 
   *                    if a function, the function is executed, and its
   *                      result determines whether to continue matching.
   *                      If the result is boolean, the boolean result
   *                      value directly indicates whether matching will
   *                      continue. If the result is a promise, a match is
   *                      indicated and the promise returned by expect is
   *                      resolved when the promise returned by the called
   *                      function resolves. If the function is associated
   *                      with a timeout value, and the function returns
   *                      false, the returned promise is rejected.
   *                    if omitted, a match is indicated, and the returned
   *                      promise is resolved.
   *                    otherwise, an error is indicated, and the returned
   *                      promise is rejected.
   *
   * Returns:
   *   A promise that is resolved when a match is indicated.
   */
  expect(patterns) {
    const me = this;
    return new Promise((resolve, reject) => {
      //
      // Pre-scan the pattern array looking for timeout specifications.
      // If any are found, remove them from the array and begin running
      // a timer. If more than one is found, the first one is used and
      // others are discarded.
      //
      let timeout = null;
      let intervalId = null;
      let i = 0;
      while (i < patterns.length) {
        let pattern = patterns[i];
        if (typeof pattern.re === "number") {
          if (timeout === null && typeof pattern.fn === "function") {
            timeout = {
              maxSeconds: pattern.re,
              fn: pattern.fn
            };
          }
          patterns.splice(i, 1);
        }
        else {
          i += 1;
        }
      }
      if (timeout === null && typeof me.defaultTimeout === "object") timeout = me.defaultTimeout;
      if (timeout !== null) {
        intervalId = setInterval(() => {
          me.streamMgr.clearInputStream();
          try {
            if (timeout.fn() === false) {
              //
              // When the timeout function returns false, this
              // indicates that the consumer should not be applied
              // to incoming text anymore, and the promise should
              // be rejected. Otherwise, if the function returns true,
              // the timeout interval will remain in effect and
              // the consumer will continue to be applied to received
              // text.
              //
              clearTimeout(intervalId);
              me.streamMgr.endConsumer();
              reject(new Error("Timeout"));
            }
          }
          catch (err) {
            clearTimeout(intervalId);
            me.streamMgr.endConsumer();
            reject(err);
          }
        }, timeout.maxSeconds * 1000);
      }
      let str = "";
      this.streamMgr.startConsumer(data => {
        if (str.length > 8192) {
          str = str.substring(str.length - 8192);
        }
        str += data;
        for (let pattern of patterns) {
          let match = str.match(pattern.re);
          if (match !== null) {
            if (this.isDebug) {
              this.log(`expect ${pattern.re} match at ${match.index} of ${str}`);
              this.log(`  ${match[0]}`);
            }
            str = str.substring(match.index + match[0].length);
            if (typeof pattern.fn === "function") {
              try {
                let res = pattern.fn();
                if (typeof res === "boolean") {
                  if (res) {
                    return true;
                  }
                  else {
                    resolve();
                  }
                }
                else if (res instanceof Promise) {
                  resolve(res);
                }
                else {
                  reject(new Error(res));
                }
              }
              catch(err) {
                reject(err);
              }
            }
            else if (typeof pattern.fn === "string" && pattern.fn === "continue") {
              return true;
            }
            else if (typeof pattern.fn !== "undefined") {
              reject(new Error(pattern.fn));
            }
            else {
              resolve();
            }
            if (intervalId !== null) clearTimeout(intervalId);
            return false;
          }
        }
        return true;
      });
    });
  }

  /*
   * log
   *
   * Display a message prefaced by a timestamp to the user.
   *
   * Arguments:
   *   message - the message to be displayed
   */
  log(message) {
    console.log(`${new Date().toLocaleTimeString()} ${message}`);
  }

  /*
   * runScript
   *
   * Run a script on the terminal emulator.
   *
   * Arguments:
   *   script - the script to run
   *
   * Returns:
   *   A promise that is resolved when the script is finished.
   */
  runScript(script) {
    return new Promise((resolve, reject) => {
      this.machine.runScript(script, () => {
        resolve();
      });
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
    this.log(`${new Date().toLocaleTimeString()} ${message}`);
    return Promise.resolve();
  }

  /*
   * send
   *
   * Send a string to the host as if it had been entered from the terminal
   * keyboard.
   *
   * Arguments:
   *   s - the string to send
   *
   * Returns:
   *   A promise that is resolved when the string has been sent.
   */
  send(s) {
    const wasQueueEmpty = this.keyQueue.length < 1;
    const limit = s.length - 1;
    const promise = new Promise((resolve, reject) => {
      for (let i = 0; i < s.length; i++) {
        let key = s.charAt(i);
        let cc = s.charCodeAt(i);
        let isShift = false;
        let isCtrl = false;
        if (cc < 0x20) {
          switch (cc) {
          case 0x08:
            key = "Backspace";
            break;
          case 0x09:
            key = "Tab";
            break;
          case 0x0d:
            key = "Enter";
            break;
          case 0x1b:
            key = "Escape";
            break;
          default:
            isCtrl = true;
            break;
          }
        }
        else if (cc >= 0x41 && cc <= 0x5a) {
          isShift = true;
        }
        else if (cc === 0x7f) {
          key = "Delete";
        }
        if (i < limit) {
          this.keyQueue.push({
            key: key,
            isShift: isShift,
            isCtrl: isCtrl,
            isAlt: false,
            delay: this.minimumKeyInterval
          });
        }
        else {
          this.keyQueue.push({
            key: key,
            isShift: isShift,
            isCtrl: isCtrl,
            isAlt: false,
            delay: this.minimumKeyInterval,
            resolver: resolve
          });
        }
      }
    });
    if (wasQueueEmpty) this.startSendingKeys();
    return promise;
  }

  /*
   * sendKey
   *
   * Send a keyboard key with modifiers to the host, as if a user had pressed the
   * indicated key combination.
   *
   * Arguments:
   *   key     - the name of the base key pressed
   *   isShift - true if shift keypress indication
   *   isCtrl  - true if control keypress indication
   *   isAlt   - true if alt keypress indication
   *   delay   - optional delay in milliseconds before sending key
   *
   * Returns:
   *   A promise that is resolved when the key has been sent.
   */
  sendKey(key, isShift, isCtrl, isAlt, delay) {
    const promise = new Promise((resolve, reject) => {
      this.keyQueue.push({
        key: key,
        isShift: isShift,
        isCtrl: isCtrl,
        isAlt: isAlt,
        delay: delay,
        resolver: resolve
      });
    });
    if (this.keyQueue.length === 1) this.startSendingKeys();
    return promise;
  }

  /*
   * sendKeyDirect
   *
   * Send a keyboard key with modifiers to the host directly (not as a promise),
   * as if a user had pressed the indicated key combination.
   *
   * Arguments:
   *   key     - the name of the base key pressed
   *   isShift - true if shift keypress indication
   *   isCtrl  - true if control keypress indication
   *   isAlt   - true if alt keypress indication
   *   delay   - optional delay in milliseconds before sending key
   */
  sendKeyDirect(key, isShift, isCtrl, isAlt, delay) {
    this.keyQueue.push({
      key: key,
      isShift: isShift,
      isCtrl: isCtrl,
      isAlt: isAlt,
      delay: delay
    });
    if (this.keyQueue.length === 1) this.startSendingKeys();
  }

  /*
   * sendList
   *
   * Send a list of strings (usually commands or text lines) to the host, waiting for
   * a specified prompt from the host after each one, and delaying by a specified
   * amount of time after each one.
   *
   * Arguments:
   *   list    - the list of strings (usually commands or text lines)
   *   prompt  - a regular expression representing the prompt for which to wait
   *             after each string is sent
   *   delay   - number of milliseconds to delay after each string
   *
   * Returns:
   *   A promise that is resolved when the whole list has been sent.
   */
  sendList(list, prompt, delay) {
    let promise = Promise.resolve();
    for (const s of list) {
      promise = promise
      .then(() => this.send(s))
      .then(() => this.expect([{ re: prompt }]))
      .then(() => this.sleep(delay));
    }
    return promise;
  }

  /*
   * setDebug
   *
   * Switch debugging mode on/off.
   *
   * Arguments
   *   isDebug - true to switch debugging mode on
   */
  setDebug(isDebug) {
    this.isDebug = isDebug;
    this.emulator.setDebug(isDebug);
  }

  /*
   * setDefaultTimeout
   *
   * Set the default maximum number of seconds to wait for a response from the host, and a
   * function to execute when a timeout occurs.
   *
   * Arguments
   *   timeout - the maximum number of seconds to wait
   *   fn      - the function to execute. If the function returns true, the timer will
   *             be restarted, and waiting will continue. Otherwise, an exception will
   *             be indicated.
   */
  setDefaultTimeout(timeout, fn) {
    this.defaultTimeout = {
      maxSeconds: timeout,
      fn: fn
    };
  }

  /*
   * setKeyInterval
   *
   * Set the minimum number of milliseconds between keys sent upline.
   *
   * Arguments
   *   interval - minimum number of milliseconds
   */
  setKeyInterval(interval) {
    this.minimumKeyInterval = interval;
  }

  /*
   * setTracer
   *
   * Set a function that traces data received from and sent to the host.
   *
   * Arguments
   *   tracer - the tracing function. If this parameter is omitted, a default tracing
   *            function is registered. The default tracing function logs data to the console.
   *            Two parameters are passed to the tracing function:
   *              direction : "R" for receive, "S" for send
   *              data      : the data received or sent
   *   strip  - an optional boolean argument, normally used with the default tracing
   *            function. If true, parity bits are stripped from bytes sent and
   *            received before logging them.
   */
  setTracer(tracer, strip) {
    this.tracerDoesStrip = false;
    if (typeof tracer === "function") {
      this.tracer = tracer;
      if (typeof strip === "boolean") {
        this.tracerDoesStrip = strip;
      }
    }
    else {
      if (typeof tracer === "boolean" && typeof strip === "undefined") {
        this.tracerDoesStrip = tracer;
      }
      let lastDirection = null;
      let text = "";
      process.on("exit", code => {
        if (text.length > 0) {
          for (const line of text.split("\n")) {
            if (line.length > 0) this.log(`${lastDirection} : ${line}`);
          }
        }
      });
      this.tracer = (direction, data) => {
        if (typeof data === "string") {
          let ab = new Uint8Array(data.length);
          for (let i = 0; i < data.length; i++) {
            ab[i] = data.charCodeAt(i) & 0xff;
          }
          data = ab;
        }
        let s = "";
        for (let i = 0; i < data.length; i++) {
          let b = data[i];
          if (this.tracerDoesStrip) b &= 0x7f;
          if (b < 0x10) {
            s += `<0${b.toString(16)}>`;
            if (b === 0x0a) s += "\n";
          }
          else if (b < 0x20 || b > 0x7e) {
            s += `<${b.toString(16)}>`;
          }
          else {
            s += String.fromCharCode(b);
          }
        }
        if (direction !== lastDirection && text.length > 0) {
          for (const line of text.split("\n")) {
            if (line.length > 0) this.log(`${lastDirection} : ${line}`);
          }
          text = "";
        }
        text += s;
        lastDirection = direction;
      };
    }
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
   * startSendingKeys
   *
   * Begin sending keystroke definitions that have been added to the keystroke queue.
   */
  startSendingKeys() {
    while (this.keyQueue.length > 0) {
      let head  = this.keyQueue[0];
      let delay = (typeof head.delay === "number") ? head.delay : 0;
      if (delay > 0) {
        setTimeout(() => {
          this.emulator.processKeyboardEvent(head.key, head.isShift, head.isCtrl, head.isAlt);
          this.keyQueue = this.keyQueue.slice(1);
          if (typeof head.resolver === "function") head.resolver();
          if (this.keyQueue.length > 0) this.startSendingKeys();
        }, delay);
        return;
      }
      else {
        this.emulator.processKeyboardEvent(head.key, head.isShift, head.isCtrl, head.isAlt);
        this.keyQueue = this.keyQueue.slice(1);
        if (typeof head.resolver === "function") head.resolver();
      }
    }
  }
}

/*
 * AnsiTerminal
 *
 * This class emulates the behavior of an ANSI X.364 terminal.
 */
class AnsiTerminal extends BaseTerminal {

  constructor() {
    super(new ATerm());
    this.isTelnetConnection = true;
    this.emulator.context.createImageData(this.emulator.fontWidth * 132, this.emulator.fontHeight * 45);
  }

  /*
   * loginNOS1
   *
   * Log into Telex on a CDC NOS 1 system
   *
   * Arguments:
   *   username - the username with which to login
   *   password - the password with which to login
   *
   * Returns:
   *   A promise that is reolved when the login is complete.
   */
  loginNOS1(username, password) {
    this.isTelnetConnection = false; // connections via 6676 mux do not have Telnet protocol
    return this.expect([{ re: /USER NUMBER/ }])
    .then(() => this.expect([{ re: /XXXXXXXXXXXXXX/ }]))
    .then(() => this.sleep(1000))
    .then(() => this.send(`${username}\r`))
    .then(() => this.expect([{ re: /PASSWORD/ }]))
    .then(() => this.sleep(1000))
    .then(() => this.send(`${password}\r`))
    .then(() => this.expect([{ re: /RECOVER \/SYSTEM/ }]))
    .then(() => this.sleep(1000))
    .then(() => this.send("full\r"))
    .then(() => this.expect([{ re: /\// }]));
  }

  /*
   * loginNOS2
   *
   * Log into NAM/IAF on a CDC NOS 2 system
   *
   * Arguments:
   *   family   - the family with which to login
   *   username - the username with which to login
   *   password - the password with which to login
   *
   * Returns:
   *   A promise that is reolved when the login is complete.
   */
  loginNOS2(family, username, password) {
    return this.expect([{ re: /FAMILY:/ }])
    .then(() => this.sleep(1000))
    .then(() => this.send(`${family},${username},${password}\r`))
    .then(() => this.expect([{ re: /\// }]));
  }
}

/*
 * CybisTerminal
 *
 * This class emulates the behavior of a CYBIS/PLATO terminal connecting to
 * NAM/PNI on a CDC NOS 2 system.
 */
class CybisTerminal extends BaseTerminal {

  constructor() {
    super(new PTerm());
    this.emulator.context.isGraphical = true;
    this.emulator.context.createImageData(512, 512);
  }

  /*
   * login
   *
   * Log into CYBIS on a CDC NOS 2 system
   *
   * Arguments:
   *   user     - the user name with which to login
   *   group    - the group name with which to login
   *   password - the password with which to login
   *
   * Returns:
   *   A promise that is reolved when the login is complete.
   */
  login(user, group, password) {
    return this.expect([
      { re: /Press  NEXT  to begin/, fn: () => {
              this.sendKeyDirect("Enter", false, false, false, 1000);
              return true;
            }
      },
      { re: /USER ACCESS NOT POSSIBLE/, fn:"CYBIS is currently rejecting logins"},
      { re: /Enter your user name, and then press NEXT/ },
      { re: 15, fn: () => {
              this.sendKeyDirect("S", true, true, false);
              this.sendKeyDirect("Enter", false, false, false, 2000);
              return true;
            }
      }
    ])
    .then(() => this.sleep(2000))
    .then(() => this.send(user))
    .then(() => this.sendKey("Enter", false, false, false, 1000))
    .then(() => this.expect([
      { re: /Enter your user group, and then press NEXT/ },
      { re: 15, fn: () => {
              this.sendKeyDirect("Enter", false, false, false);
              return true;
            }
      }
    ]))
    .then(() => this.sleep(2000))
    .then(() => this.send(group))
    .then(() => this.sendKey("Enter", false, false, false, 1000))
    .then(() => this.expect([{ re: /Enter your password, then press NEXT/ }]))
    .then(() => this.sleep(2000))
    .then(() => this.send(password))
    .then(() => this.sendKey("Enter", false, false, false, 1000))
    .then(() => this.expect([
      { re: /Incorrect password/, fn: "Incorrect password" },
      { re: /Choose a lesson.*HELP available/ },
      { re: /You have not changed your password in the last [0-9]+ days.*NEXT to continue/,
        fn: () => {
              this.sendKeyDirect("Enter", false, false, false, 2000);
              return true;
            }
      },
      { re: 15, fn: () => {
              this.sendKeyDirect("Enter", false, false, false);
              return true;
            }
      }
    ]));
  }
}

/*
 * PlatoTerminal
 *
 * This class emulates the behavior of a PLATO terminal connecting to PLATO
 * on a CDC NOS 1 system.
 */
class PlatoTerminal extends BaseTerminal {

  constructor() {
    super(new PTermClassic());
    this.setKeyInterval(250);
    this.setDefaultTimeout(10, () => false);
    this.emulator.context.isGraphical = true;
    this.emulator.context.createImageData(512, 512);
  }

  /*
   * login
   *
   * Log into PLATO on a CDC NOS 1 system
   *
   * Arguments:
   *   user     - the user name with which to login
   *   course   - the course name into which to login
   *   password - the password with which to login
   *
   * Returns:
   *   A promise that is reolved when the login is complete.
   */
  login(user, course, password) {
    const me = this;
    return this.expect([{ re: /Connected to Plato station/ }])
    .then(() => this.sleep(2000))
    .then(() => this.sendKey("S", true, true, false))
    .then(() => this.expect([
      { re: /Press  NEXT  to begin/, fn: () => {me.emulator.processKeyboardEvent("S", true, true, false); return true;} },
      { re: /Type your PLATO name, then press NEXT/ }
    ]))
    .then(() => this.sleep(1000))
    .then(() => this.send(`${user}\r`))
    .then(() => this.expect([{ re: /Type the name of your COURSE, then press SHIFT-STOP/ }]))
    .then(() => this.sleep(1000))
    .then(() => this.send(course))
    .then(() => this.sendKey("S", true, true, false))
    .then(() => this.expect([{ re: /Type your password, then press NEXT/ }]))
    .then(() => this.sleep(1000))
    .then(() => this.send(`${password}\r`))
    .then(() => this.expect([
      { re: /WRONG CODE/, fn: "Incorrect password" },
      { re: /Choose a lesson/ }
    ]));
  }
}

module.exports = { BaseTerminal, AnsiTerminal, CybisTerminal, PlatoTerminal };
