/*
 *  mode4.js
 *
 *  This module implements the MODE4 protocol atop TCP.
 */
const Const = require("./const");
const Logger = require("./logger");
const net = require("net");
const RJE = require("./rje");
const Translator = require("./translator");

class Mode4 {

  static SOH = 0x01;
  static ETX = 0x03;
  static SYN = 0x16;
  static ESC = 0x3e;
  static E1  = 0x42;
  static E2  = 0x20;
  static E3  = 0x21;
  static E4  = 0x22;

  static BLANK     = 0x20;
  static TILDE     = 0x7e;
  static TILDE_EOF = [0x7e,0x65,0x6f,0x66];
  static TILDE_EOR = [0x7e,0x65,0x6f,0x72];

  static EOF = [Mode4.ESC, 0x56,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50];
                
  static EOL = [Mode4.ESC, 0x50];

  static EOR = [Mode4.ESC, 0x57,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
                0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50];

  static StateSOH = 0;
  static StateETX = 1;

  static MsgTypePoll       = 0x05;
  static MsgTypeAck        = 0x06;
  static MsgTypeAlert      = 0x07;
  static MsgTypeResetWrite = 0x0c;
  static MsgTypeDiagWrite  = 0x10;
  static MsgTypeWrite      = 0x11;
  static MsgTypeClearWrite = 0x12;
  static MsgTypeRead       = 0x13;
  static MsgTypeError      = 0x15;
  static MsgTypeReject     = 0x18;

  static OddParity = [
    0x80,0x01,0x02,0x83,0x04,0x85,0x86,0x07,0x08,0x89,0x8a,0x0b,0x8c,0x0d,0x0e,0x8f,
    0x10,0x91,0x92,0x13,0x94,0x15,0x16,0x97,0x98,0x19,0x1a,0x9b,0x1c,0x9d,0x9e,0x1f,
    0x20,0xa1,0xa2,0x23,0xa4,0x25,0x26,0xa7,0xa8,0x29,0x2a,0xab,0x2c,0xad,0xae,0x2f,
    0xb0,0x31,0x32,0xb3,0x34,0xb5,0xb6,0x37,0x38,0xb9,0xba,0x3b,0xbc,0x3d,0x3e,0xbf,
    0x40,0xc1,0xc2,0x43,0xc4,0x45,0x46,0xc7,0xc8,0x49,0x4a,0xcb,0x4c,0xcd,0xce,0x4f,
    0xd0,0x51,0x52,0xd3,0x54,0xd5,0xd6,0x57,0x58,0xd9,0xda,0x5b,0xdc,0x5d,0x5e,0xdf,
    0xe0,0x61,0x62,0xe3,0x64,0xe5,0xe6,0x67,0x68,0xe9,0xea,0x6b,0xec,0x6d,0x6e,0xef,
    0x70,0xf1,0xf2,0x73,0xf4,0x75,0x76,0xf7,0xf8,0x79,0x7a,0xfb,0x7c,0xfd,0xfe,0x7f
  ];

  constructor(options) {
    this.handlers = {};
    this.debug = false;
    this.verbose = false;
    this.mode4 = {};
    this.highWaterMark = 64;
    this.isPrintStreamActive = false;
    this.isReady = true;
    this.lowWaterMark = 32;
    this.queuedCards = [];
    this.queuedMessages = [];
    this.siteId = 0x7a;
    this.streams = {};
    this.translator = new Translator();
    if (typeof options !== "undefined") {
      for (let key of Object.keys(options)) {
        this[key] = options[key];
      }
    }
    if (this.mode4.debug) this.debug = true;
    if (this.mode4.verbose) this.verbose = true;
    if (typeof this.mode4.siteId !== "undefined") this.siteId = this.mode4.siteId;
    if (this.debug) this.logger = new Logger("mode4");
  }

  calculateByteParity(data) {
    let result = [];
    for (const b of data) result.push(Mode4.OddParity[b & 0x7f]);
    return result;
  }

  calculateMessageParity(data) {
    let sum = 0;
    for (const b of data) sum ^= b;
    return sum ^ 0xff;
  }

  checkReadiness() {
    if (this.queuedMessages.length < this.lowWaterMark && this.isReady === false) {
      this.isReady = true;
      for (const key of Object.keys(this.streams)) this.streams[key].pause();
    }
    else if (this.queuedMessages.length >= this.highWaterMark && this.isReady) {
      this.isReady = false;
      for (const key of Object.keys(this.streams)) this.streams[key].resume();
    }
  }

  command(text) {
    let bcd = this.translator.toBcd(text);
    this.queueMessage([Mode4.MsgTypeRead].concat(bcd).concat(Mode4.EOL).concat([Mode4.ESC,Mode4.E1]));
  }

  log(msg) {
    if (this.debug) this.logger.log(msg);
  }

  logData(label, data) {
    if (this.debug) this.logger.logData(label, data);
  }

  on(evt, handler) {
    this.handlers[evt] = handler;
  }

  processMessage(message) {
    const stationId = message[1];
    const payload = message.slice(3);
    const msgType = message[2];
    switch (msgType) {
    case Mode4.MsgTypePoll:
      if (this.queuedMessages.length > 0) {
        const message = this.queuedMessages.shift();
        this.write(message, stationId);
        this.checkReadiness();
      }
      else if (this.isPrintStreamActive) {
        this.write([Mode4.MsgTypeRead].concat([Mode4.ESC,Mode4.E3]), stationId);
      }
      else {
        this.write([Mode4.MsgTypeReject], this.lastWriteStationId & 0xfe);
      }
      break;
    case Mode4.MsgTypeAlert:
      break;
    case Mode4.MsgTypeWrite:
    case Mode4.MsgTypeResetWrite:
    case Mode4.MsgTypeClearWrite:
      this.lastWriteStationId = stationId;
      let text = "";
      let E = 0;
      let i = 0;
      while (i < payload.length) {
        let b = payload[i++];
        if (b === Mode4.ESC) {
          if (i < payload.length) {
            b = payload[i++];
            if (b === Mode4.E1) {
              E = 1;
            }
            else if (b === Mode4.E2) {
              E = 2;
            }
            else if (b === Mode4.E3) {
              E = 3;
            }
            else if (b === 0x50) {
              text += "\n";
            }
            else if (b === Mode4.E4) {
              E = 4;
            }
            else if (b >= 0x23 && b <= 0x3f) {
              b &= 0x1f;
              while (b-- > 0) text += " ";
            }
            else if (b === 0x41) {
              text += "\r";
            }
            else if (b >= 0x43 && b <= 0x4f) {
              b &= 0x0f;
              while (b-- > 0) text += "0";
            }
          }
        }
        else if (b < Translator.BcdToAscii.length) {
          text += String.fromCharCode(Translator.BcdToAscii[b]);
        }
      }
      if (this.isPrintStreamActive && E !== 2) {
        if (typeof this.handlers.data === "function")
          this.handlers.data(RJE.StreamType_Printer, 1, null);
        this.isPrintStreamActive = false;
      }
      switch (E) {
      case 1:
        if (text.length > 0 && typeof this.handlers.data === "function")
          this.handlers.data(RJE.StreamType_Console, 1, text);
        break;
      case 2:
        this.isPrintStreamActive = true;
        if (typeof this.handlers.data === "function")
          this.handlers.data(RJE.StreamType_Printer, 1, text);
        break;
      case 3:
        if (this.queuedCards.length > 0) {
          this.queueMessage(this.queuedCards.shift());
        }
        break;
      }
      this.write([Mode4.MsgTypeAck], stationId);
      break;
    case Mode4.MsgTypeDiagWrite:
      this.lastWriteStationId = stationId;
      this.write([Mode4.MsgTypeAck], stationId);
      break;
    default:
      this.log(`Unrecognized message type: $(msgType)`);
      break;
    }
  }

  queueCards(cards) {
    this.queuedCards.push(cards);
    this.checkReadiness();
  }

  queueMessage(message) {
    this.queuedMessages.push(message);
    this.checkReadiness();
  }

  requestToSend(streamId) {
    if (typeof this.handlers.pti === "function") {
      this.command("READ");
      this.handlers.pti(RJE.StreamType_Reader, streamId);
    }
  }

  send(streamId, readable) {
    let buf = [];
    this.cardCount = 0;
    this.cardBlock = [];
    readable.on("data", data => {
      for (const b of data) buf.push(b);
      let limit = buf.indexOf(0x0a);
      if (limit > 0 && buf[limit - 1] === 0x0d) limit -= 1;
      while (limit >= 0) {
        let card = buf.slice(0, limit);
        if (buf[limit] === 0x0d) limit += 1;
        buf = buf.slice(limit + 1);
        limit = buf.indexOf(0x0a);
        if (limit > 0 && buf[limit - 1] == 0x0d) limit -= 1;
        if (card.length > 0 && card[0] === Mode4.TILDE) {
          if (card.length === 1
              || (card.length === 4 && card.every((b, i) => b === Mode4.TILDE_EOR[i]))) {
            this.cardBlock = this.cardBlock.concat(Mode4.EOR);
          }
          else if (card.length === 4 && card.every((b, i) => b === Mode4.TILDE_EOF[i])) {
            this.cardBlock = this.cardBlock.concat(Mode4.EOF);
          }
          else {
            continue;
          }
        }
        else {
          if (card.length > 80) card = card.slice(0, 80);
          while (card.length < 80) card = card.concat([Mode4.BLANK]);
          this.cardBlock = this.cardBlock.concat(this.translator.toBcd(card));
        }
        if (++this.cardCount >= 12) {
          this.queueCards([Mode4.MsgTypeRead].concat(this.cardBlock).concat([Mode4.ESC,Mode4.E3]));
          this.cardCount = 0;
          this.cardBlock = [];
        }
      }
    });
    readable.on("end", () => {
      this.queueCards([Mode4.MsgTypeRead].concat(this.cardBlock).concat(Mode4.EOF).concat([Mode4.ESC,Mode4.E2]));
      if (typeof this.handlers.end === "function") this.handlers.end(1);
    });
  }

  start(name, password) {
    this.isPrintStreamActive = false;
    this.queuedMessages = [];
    this.state = Mode4.StateSOH;
    this.message = [];
    this.lastWriteStationId = 0x61;
    this.socket = net.createConnection({port:this.port, host:this.host}, () => {
      this.log(`connected to ${this.host}:${this.port}`);
      if (typeof name === "string" && typeof password === "string")
        this.command(`LOGIN,${name},${password}`);
      if (typeof this.handlers.connect === "function") this.handlers.connect();
    });
    this.socket.on("data", data => {
      for (let b of data) {
        b &= 0x7f;
        if (b !== Mode4.SYN) {
          switch (this.state) {
          case Mode4.StateSOH:
            if (b === Mode4.SOH) {
              this.message = [];
              this.state = Mode4.StateETX;
            }
            break;
          case Mode4.StateETX:
            if (b === Mode4.ETX) {
              if (this.verbose || this.message[0] === this.siteId)
                this.logData("recv:", this.message);
              if (this.message.length > 2) {
                if (this.message[0] === this.siteId) {
                  this.processMessage(this.message);
                }
              }
              this.state = Mode4.StateSOH;
            }
            else {
              this.message.push(b);
            }
            break;
          }
        }
      }
    });
    this.socket.on("end", () => {
      this.log("disconnected");
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

  write(data, stationId, siteId) {
    if (typeof siteId === "undefined") siteId = this.siteId;
    data = [siteId,stationId].concat(data);
    this.logData("send:", data);
    const msg = [Mode4.SYN,Mode4.SYN,Mode4.SYN,Mode4.SYN]
                .concat(this.calculateByteParity([Mode4.SOH].concat(data).concat([Mode4.ETX])))
                .concat([0]);
    this.socket.write(new Uint8Array(msg));
  }
}

module.exports = Mode4;
