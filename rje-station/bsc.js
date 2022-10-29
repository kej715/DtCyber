/*
 *  bsc.js
 *
 *  This class simulates bisync serial data communication over a TCP
 *  connection. The implementation is compatible with DtCyber and
 *  Hercules.
 */

const Const = require("./const");
const Logger = require("./logger");
const net = require("net");

class Bsc {

  static ErrNone       = 0;
  static ErrTimeout    = 1;

  static BlockHeader   = [Const.SYN, Const.SYN, Const.SYN, Const.SYN, Const.DLE, Const.STX];
  static BlockTrailer  = [Const.DLE, Const.ETB];
  static EnqIndication = [Const.SOH, Const.ENQ];
  static AckIndication = [Const.SYN, Const.SYN, Const.SYN, Const.SYN, Const.DLE, Const.ACK0];
  static NakIndication = [Const.SYN, Const.SYN, Const.SYN, Const.SYN, Const.NAK];

  static StateDisconnected = 0;
  static StateConnecting   = 1;
  static StateConnected    = 2;
  static StateENQ_DLE      = 3;
  static StateENQ_ACK      = 4;
  static StateSoB_DLE      = 5;
  static StateSoB_STX      = 6;
  static StateEoB_DLE      = 7;
  static StateEoB_ETB      = 8;

  constructor(options) {
    this.handlers = {};
    this.state = Bsc.StateDisconnected;
    this.host = "localhost";
    this.port = 2553;
    this.debug = false;
    this.queuedBlocks = [];
    this.receivedData = [];
    this.ackDelay = 250;
    if (typeof options !== "undefined") {
      for (let key of Object.keys(options)) {
        this[key] = options[key];
      }
    }
    if (typeof this.bsc !== "undefined") {
      if (this.bsc.debug) this.debug = true;
    }
    if (this.debug) this.logger = new Logger("bsc");
  }

  log(msg) {
    if (this.debug) this.logger.log(msg);
  }

  end() {
    if (typeof this.socket !== "undefined") {
      this.socket.end();
    }
  }

  logData(label, data) {
    if (this.debug) this.logger.logData(label, data);
  }

  on(evt, handler) {
    this.handlers[evt] = handler;
  }

  sendBlock(isLastAck) {
    const provided = (typeof this.blockProvider === "function") ? this.blockProvider() : null;
    if (provided != null) {
      this.ackDelay = 250;
      let block = Array.from(Bsc.BlockHeader);
      for (let b of provided) {
        if (b === Const.DLE) {
          block = block.concat([Const.DLE, Const.DLE]);
        }
        else {
          block.push(b);
        }
      }
      block = block.concat(Bsc.BlockTrailer);
      this.write(block);
    }
    else if (isLastAck) {
      setTimeout(() => {this.write(Bsc.AckIndication);}, this.ackDelay);
      if (this.ackDelay < 2000) this.ackDelay *= 2;
    }
    else {
      this.ackDelay = 250;
      this.write(Bsc.AckIndication);
    }
  }

  setBlockProvider(provider) {
    this.blockProvider = provider;
  }

  start(callback) {
    if (this.state === Bsc.StateDisconnected) {
      this.state = Bsc.StateConnecting;
      this.socket = net.createConnection({port:this.port, host:this.host}, () => {
        this.log(`connected to ${this.host}:${this.port}`);
        this.state = Bsc.StateENQ_DLE;
        setTimeout(() => {this.write(Bsc.EnqIndication);}, 0);
      });
      this.socket.on("data", data => {
        if (typeof this.timeout !== "undefined") {
          clearTimeout(this.timeout);
          delete this.timeout;
        }
        let len = this.receivedData.length;
        for (const b of data) this.receivedData.push(b);
        this.logData("recv:", this.receivedData.slice(len));
        while (this.receivedData.length > 0) {
          const b = this.receivedData.shift();
          switch (this.state) {
          //
          // Wait for <DLE> in response to <SOH><ENQ>.
          // <SYN> is accepted and discarded.
          //
          case Bsc.StateENQ_DLE:
            if (b === Const.DLE) {
              this.state = Bsc.StateENQ_ACK;
            }
            else if (b !== Const.SYN) {
              this.receivedData = [];
              setTimeout(() => {this.write(Bsc.EnqIndication);}, 0);
            }
            break;
          //
          // Wait for <ACK0> following <DLE> in response to <SOH<<ENQ>.
          //
          case Bsc.StateENQ_ACK:
            if (b === Const.ACK0) {
              this.state = Bsc.StateSoB_DLE;
              this.queuedBlocks = [];
              if (typeof callback === "function") callback();
              this.sendBlock(false);
            }
            else {
              this.state = Bsc.StateENQ_DLE;
              this.receivedData = [];
              setTimeout(() => {this.write(Bsc.EnqIndication);}, 0);
            }
            break;
          //
          // Wait for <DLE> in start of block during normal operation.
          // <SYN> is accepted and discarded. <NAK> indicates that the
          // host rejected the last block and requests that it be
          // retransmitted.
          //
          case Bsc.StateSoB_DLE:
            if (b === Const.DLE) {
              this.state = Bsc.StateSoB_STX;
            }
            else if (b !== Const.SYN) {
              this.receivedData = [];
              if (b === Const.NAK && typeof this.lastBlockSent !== "undefined") {
                this.write(this.lastBlockSent);
              }
              else {
                this.log(`received <${b.toString(16)}> while expecting <DLE>`);
                this.write(Bsc.NakIndication);
              }
            }
            break;
          //
          // Wait for <STX> following <DLE> in start of block.
          // <ACK0> is accepted as an indication that the host
          // had nothing else to send.
          //
          case Bsc.StateSoB_STX:
            if (b === Const.ACK0) {
              this.state = Bsc.StateSoB_DLE;
              this.sendBlock(true);
            }
            else if (b === Const.STX) {
              this.receivedBlock = [];
              this.state = Bsc.StateEoB_DLE;
            }
            else {
              this.log(`received <${b.toString(16)}> while expecting <STX>`);
              this.state = Bsc.StateSoB_DLE;
              this.write(Bsc.NakIndication);
            }
            break;
          //
          // Wait for <DLE> at end of block. Accumulate bytes received
          // while waiting.
          //
          case Bsc.StateEoB_DLE:
            if (b === Const.DLE) {
              this.state = Bsc.StateEoB_ETB;
            }
            else {
              this.receivedData.push(b);
            }
            break;
          //
          // Wait for <ETB> following <DLE> at end of block.
          // If a second <DLE> is detected, this indicates a "quoted"
          // <DLE< that is appended as received data.
          //
          case Bsc.StateEoB_ETB:
            if (b === Const.ETB) {
              if (typeof this.handlers.data === "function") {
                this.handlers.data(this.receivedData);
              }
              this.state = Bsc.StateSoB_DLE;
              this.receivedData = [];
              this.sendBlock(false);
            }
            else if (b === Const.DLE) {
              this.receivedData.push(b);
              this.state = Bsc.StateEoB_DLE;
            }
            else {
              this.log(`received <${b.toString(16)}> while expecting <ETB>`);
              this.state = Bsc.StateSoB_DLE;
              this.write(Bsc.NakIndication);
            }
            break;

          default:
            console.log(`Unrecognized state: ${this.state}`);
            process.exit(1);
            break;
          }
        }
      });
      this.socket.on("end", () => {
        this.log("disconnected");
        this.state = Bsc.StateDisconnected;
      });
      this.socket.on("error", err => {
        if (typeof this.handlers.error === "function") {
          this.log(err);
          this.handlers.error(err);
        }
        else {
          throw err;
        }
      });
    }
    else {
      callback();
    }
  }

  write(data) {
    this.logData("send:", data);
    this.lastBlockSent = Array.from(data);
    this.socket.write(new Uint8Array(data));
    this.retries = 0;
    this.timeout = setTimeout(() => {
      if (++this.retries < 5) {
        if (this.state < Bsc.StateSoB_DLE) {
          this.socket.write(new Uint8Array(Bsc.EnqIndication));
        }
        else {
          this.socket.write(new Uint8Array(Bsc.NakIndication));
        }
      }
      else {
        const err = new Error("Maximum timeout/retries exceeded", {bsc: Bsc.ErrTimeout});
        if (typeof this.handlers.error === "function") {
          this.log(err);
          this.handlers.error(err);
        }
        else {
          throw err;
        }
      }
    }, 5000);
  }
}

module.exports = Bsc;
