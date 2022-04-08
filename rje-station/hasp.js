/*
 *  hasp.js
 *
 *  This module implements the HASP protocol atop the Bsc/TCP
 *  protocol provided by the bsc.js module.
 */
const Bsc = require("./bsc");
const Const = require("./const");
const Translator = require("./translator");

class Hasp {

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

  constructor(options) {
    this.handlers = {};
    this.isSignedOn = false;
    this.debug = false;
    this.hasp = {};
    this.streams = {};
    if (typeof options !== "undefined") {
      for (let key of Object.keys(options)) {
        this[key] = options[key];
      }
    }
    if (this.hasp.debug) this.debug = true;
    this.translator = new Translator();
    this.bsc = new Bsc(options);
    this.bsc.on("data", data => {
      this.receiveBlock(data);
    });
    this.bsc.on("ready", isReady => {
      if (isReady) {
        for (const key of Object.keys(this.streams)) this.streams[key].pause();
      }
      else {
        for (const key of Object.keys(this.streams)) this.streams[key].resume();
      }
    });
  }

  command(text) {
    let block = [
      0x80 | (Hasp.BlockType_Normal << 4) | (this.sndSeqNum++ & 0x0f),
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
    this.logData("send", block);
    this.bsc.queueBlock(block);
  }

  log(msg) {
    if (this.debug) {
      console.log(`${new Date().toLocaleString()} HASP ${msg}`);
    }
  }

  logData(label, data) {
    if (this.debug) {
      let msg = `${new Date().toLocaleString()} HASP ${label}`;
      for (const b of data) {
        msg += " ";
        if (b < 16) msg += "0";
        msg += b.toString(16);
      }
      console.log(msg);
    }
  }

  on(evt, handler) {
    this.handlers[evt] = handler;
    if (evt === "error") {
      this.bsc.on(evt, handler);
    }
  }

  processPostPrint(record, param) {
    let fe = " ";
    if (param > 0 && param < 0x20) {
      if (param < 0x10) { // Space NN lines after print
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
    return record + `\n${fe}`;
  }

  processPrePrint(record, param) {
    let fe = " ";
    switch ((param >> 4) & 0x03) {
    case 0: // Suppress space or post-print carriage control
      if ((param & 0x0f) == 0) { // Suppress space
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
      if ((param & 0x03) == 1) { // Page eject
        fe = "1";
      }
      break;
    }
    return `${fe}${record}\n`;
  }

  receiveBlock(data) {
    this.logData("recv:", data);
    let i = 0;
    const bcb = data[i++];
    const seqNum = bcb & 0x0f;
    const blockType = (bcb >> 4) & 0x07;
    this.fcs = (data[i] << 8) | data[i + 1];
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
          0x80 | (Hasp.BlockType_Normal << 4) | (this.sndSeqNum++ & 0x0f),
          0x8f, 0xcf,
          0x80 | (Hasp.ControlInfo_BadBCB << 4) | Hasp.RecordType_Control,
          0x80 | ((this.rcvSeqNum + 1) & 0x0f),
          0x00,
          0x00
        ];
        this.logData("send", block);
        this.bsc.queueBlock(block);
        return;
      }
    }
    while (i < data.length) {
      const rcb = data[i++];
      if (rcb === 0) break; // transmission block terminator
      let streamId = (rcb >> 4) & 0x07;
      let recordType = rcb & 0x0f;
      const srcb = data[i++];
      if (recordType === Hasp.RecordType_Control) {
        switch (streamId) {
        case Hasp.ControlInfo_RTI:
          let block = [
            0x80 | (Hasp.BlockType_Normal << 4) | (this.sndSeqNum++ & 0x0f),
            0x8f, 0xcf,
            0x80 | (Hasp.ControlInfo_PTI << 4) | Hasp.RecordType_Control,
            srcb,
            0x00,
            0x00
          ];
          this.logData("send", block);
          this.bsc.queueBlock(block);
          break;
        case Hasp.ControlInfo_PTI:
          streamId = (srcb >> 4) & 0x07;
          recordType = srcb & 0x0f;
          if (typeof this.handlers.pti === "function") {
            this.handlers.pti(recordType, streamId);
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
            this.handlers.data(recordType, streamId, null);
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
        if (recordType === Hasp.RecordType_PrintRecord) {
          const key = `LP${streamId}`;
          if (typeof this.hasp[key] === "undefined") this.hasp[key] = {};
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
          this.handlers.data(recordType, streamId, str);
        }
      }
    }
  }

  requestToSend(streamId) {
    let block = [
      0x80 | (Hasp.BlockType_Normal << 4) | (this.sndSeqNum++ & 0x0f),
      0x8f, 0xcf,
      0x80 | (Hasp.ControlInfo_RTI << 4) | Hasp.RecordType_Control,
      0x80 | (streamId << 4) | Hasp.RecordType_InputRecord,
      0x00,
      0x00
    ];
    this.logData("send", block);
    this.bsc.queueBlock(block);
  }

  send(streamId, readable) {
    let buf = [];
    const key = `CR${streamId}`;
    this.streams[key] = readable;
    readable.on("data", data => {
      for (const b of data) buf.push(b);
      let limit = buf.indexOf(0x0a);
      if (limit > 0 && buf[limit - 1] === 0x0d) limit -= 1;
      if (limit >= 0) {
        let block = [
          0x80 | (Hasp.BlockType_Normal << 4) | (this.sndSeqNum++ & 0x0f),
          0x8f, 0xcf
        ];
        while (limit >= 0) {
          block.push(0x80 | (streamId << 4) | Hasp.RecordType_InputRecord, 0x80);
          let i = 0;
          while (i < limit) {
            let len = limit - i;
            if (len > 0x3f) len = 0x3f;
            block.push(0xc0 | len);
            while (len-- > 0) {
              block.push(Translator.AsciiToEbcdic[buf[i++]]);
            }
          }
          block.push(0);
          if (buf[i] === 0x0d) i += 1;
          i += 1;
          buf = buf.slice(i);
          limit = buf.indexOf(0x0a);
          if (limit > 0 && buf[limit - 1] == 0x0d) limit -= 1;
        }
        block.push(0);
        this.logData("send", block);
        this.bsc.queueBlock(block);
      }
    });
    readable.on("end", () => {
      let block = [
        0x80 | (Hasp.BlockType_Normal << 4) | (this.sndSeqNum++ & 0x0f),
        0x8f, 0xcf,
        0x80 | (streamId << 4) | Hasp.RecordType_InputRecord,
        0x80,
        0x00,
        0x00
      ];
      this.logData("send", block);
      this.bsc.queueBlock(block);
      delete this.streams[key];
      if (typeof this.handlers.end === "function") this.handlers.end(streamId);
    });
  }

  start(name, password) {
    this.bsc.start(() => {
      this.rcvSeqNum = 0;
      this.sndSeqNum = 0;
      let card = "/*SIGNON       ";
      if (typeof name === "string") card += name;
      while (card.length < 24) card += " ";
      if (typeof password === "string") card += password;
      while (card.length < 80) card += " ";
      let block = [0xa0, 0x8f, 0xcf, 0xf0, 0xc1].concat(this.translator.toEbcdic(card)).concat([0]);
      this.logData("send", block);
      this.bsc.queueBlock(block);
      if (typeof this.handlers.signon === "function") this.handlers.signon();
    });
  }
}

module.exports = Hasp;
