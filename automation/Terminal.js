const net = require("net");

/*
 * RenderingContext
 *
 * This class implements the interface defined by the 2D rendering context of the
 * HTML canvas element. It enables capturing graphics actions executed by a browser-based
 * terminal emulator.
 */
class RenderingContext {

  constructor() {
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
    this.receivedData = Buffer.alloc(0);
  }

  appendData(data) {
    this.receivedData = Buffer.concat([this.receivedData, data]);
console.log(`Receive: ${data}`);
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
 * Terminal
 *
 * This is the main class of the module. It provides a collection of methods facilitating
 * interaction with DtCyber.
 */
class Terminal {

  constructor() {
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
    this.streamMgr = new StreamMgr();
    const doConnect = callback => {
      me.socket = net.createConnection({port:port, host:"127.0.0.1"}, () => {
        me.isConnected = true;
        callback(null);
      });
      me.streamMgr.setOutputStream(me.socket);
      me.socket.on("data", data => {
        me.streamMgr.appendData(data);
      });
      me.socket.on("close", () => {
        if (me.isConnected && me.isExitOnClose) {
          console.log(`${new Date().toLocaleTimeString()} host disconnected`);
          process.exit(0);
        }
        me.isConnected = false;
      });
      me.socket.on("error", err => {
        if (me.isConnected) {
          console.log(`${new Date().toLocaleTimeString()} ${err}`);
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
   *                    be applied to the stream
   *                fn: if string "continue", other patterns in the array
   *                    continue to be applied. This provides a way to 
   *                    recognize an intermediate pattern in the stream
   *                    and continue looking for other patterns. 
   *                    if a function, the function is executed and a match
   *                    is indicated.
   *                    if ommitted, a match is indicated.
   *                    otherwise, an error is indicated
   *
   * Returns:
   *   A promise that is resolved when a match is indicated.
   */
  expect(patterns) {
    const me = this;
    return new Promise((resolve, reject) => {
      let str = "";
      this.streamMgr.startConsumer(data => {
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
   * Send a string to DtCyber.
   *
   * Arguments:
   *   s       - the string to send
   */
  send(s) {
    this.streamMgr.write(s);
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
}

module.exports = Terminal;
