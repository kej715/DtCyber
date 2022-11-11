/*
 *  hasp.js
 *
 *  This module implements the HASP protocol atop the Bsc/TCP
 *  protocol provided by the bsc.js module.
 */
const Bsc = require("./bsc");
const Const = require("./const");
const Logger = require("./logger");
const RJE = require("./rje");
const Translator = require("./translator");

class Hasp {

  static TILDE     = 0x7e;
  static TILDE_EOF = [0x7e,0x65,0x6f,0x66];
  static TILDE_EOI = [0x7e,0x65,0x6f,0x69];
  static TILDE_EOR = [0x7e,0x65,0x6f,0x72];

  static EOF       = [0x2f,0x2a,0x45,0x4f,0x46]; // /*EOF
  static EOI       = [0x2f,0x2a,0x45,0x4f,0x49]; // /*EOI
  static EOR       = [0x2f,0x2a,0x45,0x4f,0x52]; // /*EOR

  static BlockType_Normal         = 0;
  static BlockType_IgnoreSeqNum   = 1;
  static BlockType_ResetSeqNum    = 2;

  static RecordType_Control       = 0;
  static RecordType_OpMsg         = 1;
  static RecordType_OpCmd         = 2;
  static RecordType_InputRecord   = 3;
  static RecordType_PrintRecord   = 4;
  static RecordType_PunchRecord   = 5;
  static RecordType_DatasetRecord = 6;
  static RecordType_TermMsg       = 7;

  static ControlInfo_RTI          = 1; // Request to Initiate
  static ControlInfo_PTI          = 2; // Permission to Initiate
  static ControlInfo_BadBCB       = 6;
  static ControlInfo_GCR          = 7; // General Control Record

  static RecordTypes = [ // indexed by record type
    "",
    "CO",
    "CO",
    "CR",
    "LP",
    "CP",
    "DS",
    "TM"
  ];

  static StreamTypes = [ // indexed by record type
    -1,
    RJE.StreamType_Console,
    RJE.StreamType_Console,
    RJE.StreamType_Reader,
    RJE.StreamType_Printer,
    RJE.StreamType_Punch,
    -1,
    -1
  ];

  constructor(options) {
    this.handlers = {};
    this.isSignedOn = false;
    this.debug = false;
    this.hasp = {};
    this.highWaterMark = 64;
    this.isReady = true;
    this.lowWaterMark = 32;
    this.queuedBlocks = [];
    this.streams = {};
    if (typeof options !== "undefined") {
      for (let key of Object.keys(options)) {
        this[key] = options[key];
      }
    }
    if (this.hasp.debug) this.debug = true;
    if (this.debug) this.logger = new Logger("hasp");
    if (typeof this.hasp.maxBlockSize === "undefined") this.hasp.maxBlockSize = 400;
    this.translator = new Translator();
    this.bsc = new Bsc(options);
    this.bsc.on("data", data => {
      this.receiveBlock(data);
    });
    this.bsc.setBlockProvider(() => {
      return this.provideBlock();
    });
  }

  checkReadiness() {
    if (this.queuedBlocks.length < this.lowWaterMark && this.isReady === false) {
      this.isReady = true;
      for (const key of Object.keys(this.streams)) this.streams[key].pause();
    }
    else if (this.queuedBlocks.length >= this.highWaterMark && this.isReady) {
      this.isReady = false;
      for (const key of Object.keys(this.streams)) this.streams[key].resume();
    }
  }

  command(text) {
    let block = [
      0x80 | (Hasp.BlockType_Normal << 4),
      0x8f, 0xcf,
      0x80 | (1 << 4) | Hasp.RecordType_OpCmd,
      0x80
    ];
    let i = 0;
    while (i < text.length) {
      let len = text.length - i;
      if (len > 0x3f) len = 0x3f;
      block.push(0xc0 | len);
      while (len-- > 0) {
        block.push(Translator.AsciiToEbcdic[text.charCodeAt(i++)]);
      }
    }
    block.push(0);
    block.push(0);
    this.queueBlock(block);
  }

  end() {
    this.bsc.end();
  }

  log(msg) {
    if (this.debug) this.logger.log(msg);
  }

  logData(label, data) {
    if (this.debug) this.logger.logData(label, data);
  }

  on(evt, handler) {
    this.handlers[evt] = handler;
    if (evt === "error") {
      this.bsc.on(evt, handler);
    }
  }

  processPostPrint(record, param) {
    let fe = " ";
    if (param >= 0 && param < 0x20) {
      if (param === 0) {
        fe = "+";
      }
      else if (param < 0x10) { // Space NN lines after print
        param &= 0x03;
        if (param === 3) {
          fe = "-";
        }
        else if (param === 2) {
          fe = "0";
        }
      }
      else  { // Skip to channel NNNN after print
        param &= 0x0f;
        if (param === 1) {
          fe = "1";
        }
      }
    }
    return `${record}\n${fe}`;
  }

  processPrePrint(record, param) {
    let fe = " ";
    switch ((param >> 4) & 0x03) {
    case 0: // Suppress space or post-print carriage control
      if ((param & 0x0f) === 0) { // Suppress space
        fe = "+";
      }
      break;
    case 2: // Space immediately NN spaces
      switch (param & 0x03) {
      case 3:
        fe = "-";
        break;
      case 2:
        fe = "0";
        break;
      }
      break;
    case 3: // Skip immediately to channel NN
      if ((param & 0x03) === 1) { // Page eject
        fe = "1";
      }
      break;
    }
    return `${fe}${record}\n`;
  }

  provideBlock() {
    let i = 0;
    let block = null;
    while (i < this.queuedBlocks.length) {
      block = this.queuedBlocks[i];
      if ((block[0] & 0x70) !== 0) break; // not a normal block
      const rcb = block[3];
      if (   (rcb & 0x80) === 0 // end of transmission block
          || (rcb & 0x0f) !== Hasp.RecordType_InputRecord) {
        block[0] = (block[0] & 0xf0) | this.sndSeqNum;
        this.sndSeqNum = (this.sndSeqNum + 1) & 0x0f;
        break;
      }
      const streamId = (rcb >> 4) & 0x07;
      const shiftBase = (streamId < 5) ? 12 : 8;
      if ((this.fcs & 0x4000) === 0
          && ((1 << (shiftBase - streamId)) & this.fcs) !== 0) {
        block[0] = (block[0] & 0xf0) | this.sndSeqNum;
        this.sndSeqNum = (this.sndSeqNum + 1) & 0x0f;
        break;
      }
      i += 1;
    }
    if (i < this.queuedBlocks.length) {
      this.queuedBlocks.splice(i, 1);
      this.checkReadiness();
      this.logData("send", block);
      return block;
    }
    else {
      return null;
    }
  }

  queueBlock(block) {
    this.queuedBlocks.push(block);
    this.checkReadiness();
  }

  receiveBlock(data) {
    this.logData("recv:", data);
    let i = 0;
    const bcb = data[i++];
    const seqNum = bcb & 0x0f;
    const blockType = (bcb >> 4) & 0x07;
    this.fcs = (data[i] << 8) | data[i + 1];
    this.log(`blockType:${blockType}, seqNum:${seqNum}, fcs:${this.fcs.toString(16)}`);
    i += 2;
    if (blockType === Hasp.BlockType_ResetSeqNum) {
      this.rcvSeqNum = (seqNum - 1) & 0x0f;
    }
    else if (blockType === Hasp.BlockType_Normal) {
      if (((this.rcvSeqNum + 1) & 0x0f) === seqNum) {
        this.rcvSeqNum = seqNum;
      }
      else if (this.rcvSeqNum === seqNum) { // duplicate
        return;
      }
      else { // Bad sequence number, indicate the one expected
        let block = [
          0x80 | (Hasp.BlockType_Normal << 4),
          0x8f, 0xcf,
          0x80 | (Hasp.ControlInfo_BadBCB << 4) | Hasp.RecordType_Control,
          0x80 | ((this.rcvSeqNum + 1) & 0x0f),
          0x00,
          0x00
        ];
        this.queueBlock(block);
        return;
      }
    }
    while (i < data.length) {
      const rcb = data[i++];
      if (rcb === 0) break; // transmission block terminator
      let streamId = (rcb >> 4) & 0x07;
      let recordType = rcb & 0x0f;
      const srcb = data[i++];
      this.log(`streamId:${streamId}, recordType:${recordType}, srcb:${srcb.toString(16)}`);
      if (recordType === Hasp.RecordType_Control) {
        switch (streamId) {
        case Hasp.ControlInfo_RTI:
          let block = [
            0x80 | (Hasp.BlockType_Normal << 4),
            0x8f, 0xcf,
            0x80 | (Hasp.ControlInfo_PTI << 4) | Hasp.RecordType_Control,
            srcb,
            0x00,
            0x00
          ];
          this.queueBlock(block);
          break;
        case Hasp.ControlInfo_PTI:
          streamId = (srcb >> 4) & 0x07;
          recordType = srcb & 0x0f;
          if (typeof this.handlers.pti === "function") {
            this.handlers.pti(Hasp.StreamTypes[recordType], streamId);
          }
          break;
        case Hasp.ControlInfo_BadBCB:
          // TODO: handle Bad BCB
          break;
        default:
          this.log(`unrecognized control info: ${streamId}`);
          break;
        }
      }
      else {
        //
        // It's a record for a stream. SCB's should follow.
        // Use them to reconstruct the content of the record.
        //
        const key = `${Hasp.RecordTypes[recordType]}${streamId}`;
        if (typeof this.hasp[key] === "undefined") this.hasp[key] = {};
        let str = "";
        if (i + 1 < data.length && data[i] === 0 && data[i + 1] === 0) {
          //
          // EOF is indicated by an SCB that is 0, followed immediately by
          // an RCB that is also 0
          //
          if (typeof this.handlers.data === "function") {
            this.handlers.data(Hasp.StreamTypes[recordType], streamId, null);
            delete this.hasp[key].recordCount;
          }
          break;
        }
        while (i < data.length) {
          const scb = data[i++];
          if ((scb & 0x80) === 0) break;
          if ((scb & 0x40) === 0) { // duplicate character string
            let len = scb & 0x1f;
            let c = " ";
            if ((scb & 0x20) !== 0 && i < data.length) { // duplicate character is not blank
              c = String.fromCharCode(Translator.EbcdicToAscii[data[i++]]);
            }
            while (len-- > 0) str += c;
          }
          else {
            let len = scb & 0x3f;
            while (i < data.length && len-- > 0) {
              str += String.fromCharCode(Translator.EbcdicToAscii[data[i++]]);
            }
          }
        }
        this.log(str);
        if (recordType === Hasp.RecordType_PrintRecord) {
          if (typeof this.hasp[key].isPostPrint === "undefined") this.hasp[key].isPostPrint = true;
          const isPostPrint = this.hasp[key].isPostPrint;
          if (typeof this.hasp[key].recordCount === "undefined") {
            this.hasp[key].recordCount = 0;
            if (isPostPrint) str = ` ${str}`;
          }
          if (isPostPrint) {
            str = this.processPostPrint(str, srcb & 0x7f);
          }
          else {
            str = this.processPrePrint(str, srcb & 0x7f);
          }
          this.hasp[key].recordCount += 1;
        }
        else if (recordType === Hasp.RecordType_PunchRecord) {
          str += "\n";
        }
        if (typeof this.handlers.data === "function") {
          this.handlers.data(Hasp.StreamTypes[recordType], streamId, str);
        }
      }
    }
  }

  requestToSend(streamId) {
    let block = [
      0x80 | (Hasp.BlockType_Normal << 4),
      0x8f, 0xcf,
      0x80 | (Hasp.ControlInfo_RTI << 4) | Hasp.RecordType_Control,
      0x80 | (streamId << 4) | Hasp.RecordType_InputRecord,
      0x00,
      0x00
    ];
    this.queueBlock(block);
  }

  send(streamId, readable) {
    let buf = [];
    let block = [0x80 | (Hasp.BlockType_Normal << 4), 0x8f, 0xcf];
    const key = `CR${streamId}`;
    this.streams[key] = readable;
    readable.on("data", data => {
      for (const b of data) buf.push(b);
      let limit = buf.indexOf(0x0a);
      if (limit > 0 && buf[limit - 1] === 0x0d) limit -= 1;
      if (limit >= 0) {
        while (limit >= 0) {
          let card = buf.slice(0, limit);
          if (buf[limit] === 0x0d) limit += 1;
          if (card.length > 80) card = card.slice(0, 80);
          if (card.length > 0 && card[0] === Hasp.TILDE) {
            if (card.length === 1
                || (card.length === 4 && card.every((b, i) => b === Hasp.TILDE_EOR[i]))) {
              card = Hasp.EOR;
            }
            else if (card.length === 4 && card.every((b, i) => b === Hasp.TILDE_EOF[i])) {
              card = Hasp.EOF;
            }
            else if (card.length === 4 && card.every((b, i) => b === Hasp.TILDE_EOI[i])) {
              card = Hasp.EOI;
            }
            else {
              buf = buf.slice(limit + 1);
              limit = buf.indexOf(0x0a);
              continue;
            }
          }
          else if (card.length < 1) {
            card = [0x20];
          }
          let frags = [0x80 | (streamId << 4) | Hasp.RecordType_InputRecord, 0x80];
          let i = 0;
          while (i < card.length) {
            let len = card.length - i;
            if (len > 0x3f) len = 0x3f;
            frags.push(0xc0 | len);
            while (len-- > 0) {
              frags.push(Translator.AsciiToEbcdic[card[i++]]);
            }
          }
          frags.push(0);
          if (block.length + frags.length + 8 > this.hasp.maxBlockSize) {
            block.push(0);
            this.queueBlock(block);
            block = [0x80 | (Hasp.BlockType_Normal << 4), 0x8f, 0xcf];
          }
          else {
            block = block.concat(frags);
            buf = buf.slice(limit + 1);
            limit = buf.indexOf(0x0a);
            if (limit > 0 && buf[limit - 1] == 0x0d) limit -= 1;
          }
        }
      }
    });
    readable.on("end", () => {
      if (block.length > 3) { // flush accumulated block
        block.push(0);
        this.queueBlock(block);
      }
      // Queue end of file indication
      this.queueBlock([
        0x80 | (Hasp.BlockType_Normal << 4),
        0x8f, 0xcf,
        0x80 | (streamId << 4) | Hasp.RecordType_InputRecord,
        0x80,
        0x00,
        0x00
      ]);
      delete this.streams[key];
      if (typeof this.handlers.end === "function") this.handlers.end(streamId);
    });
  }

  start(name, password) {
    this.bsc.start(() => {
      this.queuedBlocks = [];
      this.rcvSeqNum = 0x0f;
      this.sndSeqNum = 0;
      let card = "/*SIGNON       ";
      if (typeof name === "string") card += name;
      while (card.length < 24) card += " ";
      if (typeof password === "string") card += password;
      while (card.length < 80) card += " ";
      let block = [0xa0, 0x8f, 0xcf, 0xf0, 0xc1].concat(this.translator.toEbcdic(card)).concat([0]);
      this.queueBlock(block);
      if (typeof this.handlers.connect === "function") this.handlers.connect();
    });
  }
}

module.exports = Hasp;
