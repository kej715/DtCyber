/*--------------------------------------------------------------------------
**
**  Copyright (c) 2020, Kevin Jordan
**
**  indfile.js
**    This class implements the IBM IND$FILE transfer protocol.
**
**--------------------------------------------------------------------------
*/

class IndFileTransceiver {

  constructor(machine) {

    this.debug   = false;
    this.machine = machine;
    //
    // Download states
    //
    this.ST_DL_CMD_ACK    = 0; // await command acknowledgement frame
    this.ST_DL_DATA_IND   = 1; // await data indication frame
    this.ST_DL_COMPLETION = 2; // await normal completion frame
    this.ST_DL_ABORT      = 3; // await abort frame
    //
    // Upload states
    //
    this.ST_UL_CMD_ACK    = 0; // await command acknowledgement frame
    this.ST_UL_DATA_REQ   = 1; // await data request frame
    this.ST_UL_COMPLETION = 2; // await normal completion frame
    this.ST_UL_ABORT      = 3; // await abort frame
    //
    // Message types
    //
    this.MT_DATA_INDICATION    = 0xc1;
    this.MT_DATA_REQUEST       = 0xc2;
    this.MT_CONTROL_CODE       = 0xc3;
    this.MT_RETRANSMIT_REQUEST = 0x4c;
    //
    // Maximum number of bytes to send in a message during upload.
    //
    this.MAX_UL_DATA_SIZE      = 1000;
    //
    // Special character codes
    //
    this.CR  = 0x5c;
    this.EOF = 0xa9;
    //
    // Table for decoding sequence numbers, length fields, and checksum values
    //
    this.decode6BitTable = [
    /* 00 - 0f */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 10 - 1f */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 20 - 2f */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 30 - 3f */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 40 - 4f */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,28, 0, 0,31, 0,
    /* 50 - 5f */ 26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 60 - 6f */ 27, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,29, 0, 0, 0, 0,
    /* 70 - 7f */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,30, 0, 0, 0, 0, 0,
    /* 80 - 8f */  0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0,
    /* 90 - 9f */  0, 9,10,11,12,13,14,15,16,17, 0, 0, 0, 0, 0, 0,
    /* a0 - af */  0, 0,18,19,20,21,22,23,24,25, 0, 0, 0, 0, 0, 0,
    /* b0 - bf */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* c0 - cf */  0,32,33,34,35,36,37,38,39,40, 0, 0, 0, 0, 0, 0,
    /* d0 - df */  0,41,42,43,44,45,46,47,48,49, 0, 0, 0, 0, 0, 0,
    /* e0 - ef */  0, 0,50,51,52,53,54,55,56,57, 0, 0, 0, 0, 0, 0,
    /* f0 - ff */ 58,59,60,61,62,63, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    ];
    //
    // Table for encoding sequence numbers, length fields, and checksum values
    //
    this.encode6BitTable = [
    /*  0 - 15 */ 0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
    /* 16 - 31 */ 0x98,0x99,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0x50,0x60,0x4b,0x6b,0x7a,0x4e,
    /* 32 - 47 */ 0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
    /* 48 - 63 */ 0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xf0,0xf1,0xf2,0xf3,0xf4,0xf5
    ];
    //
    // Quadrant tables for decoding downloaded data
    //
    this.quadrantKeys = [0x5e, 0x7e, 0x5c, 0x7d];
    this.quadrantAlphas = [
      0x40,                                              // blank
      0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,      // A - I
      0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,      // J - R
      0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,           // S - Z
      0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,      // a - i
      0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,      // j - r
      0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,           // s - z
      0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9, // 0 - 9
      0x6c,0x50,0x6d,0x4d,0x5d,0x4c,0x4e,0x6b,0x60,0x4b, // %&_()<+,-.
      0x61,0x7a,0x6e,0x6f                                // /:>?
    ];
    this.quadrants = [
      [   // Quadrant 0 (EBCDIC)
          0x40,0xc1,0xc2,0xc3, 0xc4,0xc5,0xc6,0xc7, 0xc8,0xc9,0xd1,0xd2,
          0xd3,0xd4,0xd5,0xd6, 0xd7,0xd8,0xd9,0xe2, 0xe3,0xe4,0xe5,0xe6,
          0xe7,0xe8,0xe9,0x81, 0x82,0x83,0x84,0x85, 0x86,0x87,0x88,0x89,
          0x91,0x92,0x93,0x94, 0x95,0x96,0x97,0x98, 0x99,0xa2,0xa3,0xa4,
          0xa5,0xa6,0xa7,0xa8, 0xa9,0xf0,0xf1,0xf2, 0xf3,0xf4,0xf5,0xf6,
          0xf7,0xf8,0xf9,0x6c, 0x50,0x6d,0x4d,0x5d, 0x4c,0x4e,0x6b,0x60,
          0x4b,0x61,0x7a,0x6e, 0x6f
      ],
      [   // Quadrant 1 (ASCII)
          0x20,0x41,0x42,0x43, 0x44,0x45,0x46,0x47, 0x48,0x49,0x4a,0x4b,
          0x4c,0x4d,0x4e,0x4f, 0x50,0x51,0x52,0x53, 0x54,0x55,0x56,0x57,
          0x58,0x59,0x5a,0x61, 0x62,0x63,0x64,0x65, 0x66,0x67,0x68,0x69,
          0x6a,0x6b,0x6c,0x6d, 0x6e,0x6f,0x70,0x71, 0x72,0x73,0x74,0x75,
          0x76,0x77,0x78,0x79, 0x7a,0x30,0x31,0x32, 0x33,0x34,0x35,0x36,
          0x37,0x38,0x39,0x25, 0x26,0x27,0x28,0x29, 0x2a,0x2b,0x2c,0x2d,
          0x2e,0x2f,0x3a,0x3b, 0x3f
      ],
      [   // Quadrant 2 (OTHER 1)
          0x00,0x00,0x01,0x02, 0x03,0x04,0x05,0x06, 0x07,0x08,0x09,0x0a,
          0x0b,0x0c,0x0d,0x0e, 0x0f,0x10,0x11,0x12, 0x13,0x14,0x15,0x16,
          0x17,0x18,0x19,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
          0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
          0x00,0x00,0x00,0x00, 0x00,0x3c,0x3d,0x3e, 0x00,0xfa,0xfb,0xfc,
          0xfd,0xfe,0xff,0x7b, 0x7c,0x7d,0x7e,0x7f, 0x1a,0x1b,0x1c,0x1d,
          0x1e,0x1f,0x00,0x00, 0x00
      ],
      [   // Quadrant 3 (OTHER 2)
          0x00,0xa0,0xa1,0xea, 0xeb,0xec,0xed,0xee, 0xef,0xe0,0xe1,0xaa,
          0xab,0xac,0xad,0xae, 0xaf,0xb0,0xb1,0xb2, 0xb3,0xb4,0xb5,0xb6,
          0xb7,0xb8,0xb9,0x80, 0x00,0xca,0xcb,0xcc, 0xcd,0xce,0xcf,0xc0,
          0x00,0x8a,0x8b,0x8c, 0x8d,0x8e,0x8f,0x90, 0x00,0xda,0xdb,0xdc,
          0xdd,0xde,0xdf,0xd0, 0x00,0x00,0x21,0x22, 0x23,0x24,0x5b,0x5c,
          0x00,0x5e,0x5f,0x00, 0x9c,0x9d,0x9e,0x9f, 0xba,0xbb,0xbc,0xbd,
          0xbe,0xbf,0x9a,0x9b, 0x00
      ],
      [   // Substitute for Quadrant 0 in text mode during download
          0x40,0xc1,0xc2,0xc3, 0xc4,0xc5,0xc6,0xc7, 0xc8,0xc9,0xd1,0xd2,
          0xd3,0xd4,0x5b,0xd6, 0xd7,0x5e,0xd9,0xe2, 0xe3,0xe4,0x5d,0xe6,
          0xe7,0xe8,0xe9,0x81, 0x82,0x83,0x84,0x85, 0x86,0x87,0x88,0x89,
          0x91,0x92,0x93,0x94, 0x95,0x96,0x97,0x98, 0x99,0xa2,0xa3,0xa4,
          0xa5,0xa6,0xa7,0xa8, 0xa9,0xf0,0xf1,0xf2, 0xf3,0xf4,0xf5,0xf6,
          0xf7,0xf8,0xf9,0x6c, 0x50,0x6d,0x4d,0x7c, 0x4c,0x4e,0x6b,0x60,
          0x4b,0x61,0x7a,0x6e, 0x6f
      ]
    ];

    this.init();
  }

  init() {
    this.lastMsg = [];           // last message sent on upload
    this.sendSeqNum = 1;         // initial packet sequence number to send
    this.recvSeqNum = 0;         // initial packet sequence number to receive
  }

  toHex(b) {
    return (b > 0x0f) ? b.toString(16) : `0${b.toString(16)}`;
  }

  logData(label, data) {
    let line = label;
    let prefix = "";
    for (let i = 0; i < label.length; i++) prefix += " ";
    for (let b of data) {
      line += `${this.toHex(b)} `;
      if (line.length > 120) {
        console.log(line);
        line = prefix;
      }
    }
    if (line !== prefix) console.log(line);
  }

  formatSize(size) {
    if (size < 1024) {
      return size;
    }
    else if (size < 1024*1024) {
      return `${new Intl.NumberFormat().format(size/1024)}K`;
    }
    else {
      return `${new Intl.NumberFormat().format(size/(1024*1024))}M`;
    }
  }

  verifyCksum(data, start, len, cksum) {
    let ck = 0;
    while (len-- > 0) ck ^= data[start++];
    return (ck & 0x3f) === cksum;
  }

  terminateTransfer(msg) {
    if (typeof this.errorCallback === "function") this.errorCallback(msg);
    if (typeof this.messageHandler === "function") {
      this.terminal.removeTerminalStatusHandler(this.messageHandler);
      delete this.messageHandler;
    }
  }

  sendAbortAck(seqNum) {
    this.terminal.sendAIDWithData(this.terminal.AID_PF2, [0x11, 0x5d, 0x7b, 0xc3, seqNum, 0x81, 0x81]);
  }
  
  processDownloadMsg() {
    let me = this;
    let buffer = this.terminal.getBuffer();
    let msgType = buffer[0];
    let seqNum = this.decode6BitTable[buffer[1]];
    switch (this.state) {
    case this.ST_DL_CMD_ACK: // await command acknowledgement frame
      if (this.debug)
        console.log(`IndFileTransceiver.receive: ST_DL_CMD_ACK, msgType ${this.toHex(msgType)}, seqNum ${seqNum} (expected ${this.recvSeqNum}), status ${this.toHex(buffer[2])}${this.toHex(buffer[3])}`);
      if (msgType === this.MT_CONTROL_CODE && seqNum === this.recvSeqNum && ((buffer[2] << 8) | buffer[3]) === 0x8181) {
        this.recvSeqNum = (this.recvSeqNum + 1) & 0x3f;
        this.state = this.ST_DL_DATA_IND;
        this.terminal.sendAID(this.terminal.AID_ENTER);
      }
      break;
    case this.ST_DL_DATA_IND: // await data indication frame
      let cksum = this.decode6BitTable[buffer[2]];
      let len = (this.decode6BitTable[buffer[3]] << 6) | this.decode6BitTable[buffer[4]];
      if (this.verifyCksum(buffer, 5, len, cksum) === false) {
        console.log("IndFileTransceiver.receive: ST_DL_DATA_IND, cksum verification failed");
        this.terminal.sendAIDWithData(this.terminal.AID_PF1, [0x11, 0x5d, 0x7b, 0x4c, seqNum, 0x81, 0x81]);
      }
      else if (seqNum !== this.recvSeqNum) {
        console.log(`IndFileTransceiver.receive: ST_DL_DATA_IND, invalid seqnum, recvd ${seqNum}, expected ${this.recvSeqNum}`);
        this.state = this.ST_DL_ABORT;
        this.sendAbortAck(seqNum);
      }
      else if (msgType === this.MT_DATA_INDICATION) {
        if (this.debug)
          console.log(`IndFileTransceiver.receive: ST_DL_DATA_IND, seqnum ${seqNum}, len ${len}`);
        this.recvSeqNum = (this.recvSeqNum + 1) & 0x3f;
        if (buffer[5] === this.CR && buffer[6] === this.EOF) {
          this.state = this.ST_DL_COMPLETION;
          this.terminal.sendAID(this.terminal.AID_ENTER);
        }
        else if (len > 0) {
          let i = 5;
          let limit = i + len;
          if (this.debug) this.logData("IndFileTransceiver.receive: ST_DL_DATA_IND, ", buffer.slice(5, limit));
          while (i < limit) {
            let b = buffer[i++];
            let idx = this.quadrantAlphas.indexOf(b);
            if (idx >= 0) {
              this.fileData.push(this.quadrants[this.quadrant][idx]);
            }
            else {
              this.quadrant = this.quadrantKeys.indexOf(b);
              if (this.quadrant === 0 && this.isText) {
                this.quadrant = 4;
              }
              else if (this.quadrant < 0) {
                console.log(`IndFileTransceiver.receive: ST_DL_DATA_IND, invalid quadrant key ${this.toHex(b)}, index ${i - 1}, len ${limit}`);
                this.logData("    ", buffer.slice(0, limit));
                this.state = this.ST_DL_ABORT;
                this.sendAbortAck(seqNum);
              }
            }
          }
          if (this.state !== this.ST_DL_ABORT) {
            if (typeof this.progressCallback === "function")
              this.progressCallback(`${this.formatSize(this.fileData.length)} bytes`);
            this.terminal.sendAID(this.terminal.AID_ENTER);
          }
        }
      }
      else if (msgType === this.MT_CONTROL_CODE) {
        this.state = this.ST_DL_ABORT;
        this.processDownloadMsg();
      }
      else {
        console.log(`IndFileTransceiver.receive: ST_DL_DATA_IND, invalid msgType ${this.toHex(msgType)} in state ${this.state}`);
        this.state = this.ST_DL_ABORT;
        this.sendAbortAck(seqNum);
      }
      break;
    case this.ST_DL_COMPLETION: // await normal completion frame
      if (msgType === this.MT_CONTROL_CODE && seqNum === this.recvSeqNum && ((buffer[2] << 8) | buffer[3]) === 0x8189) {
        if (this.debug) console.log("ST_DL_COMPLETION");
        this.recvSeqNum = (this.recvSeqNum + 1) & 0x3f;
        this.terminal.removeTerminalStatusHandler(this.messageHandler);
        delete this.messageHandler;
        this.terminal.sendAID(this.terminal.AID_ENTER);
        if (typeof this.successCallback === "function") this.successCallback(this.fileData);
      }
      else {
        console.log(`IndFileTransceiver.receive: ST_DL_COMPLETION, invalid msg, msgType ${this.toHex(msgType)}, seqNum ${seqNum} (expected ${this.recvSeqNum}), status ${this.toHex(buffer[2])}${this.toHex(buffer[3])}`);
        this.state = this.ST_DL_ABORT;
        this.sendAbortAck(seqNum);
      }
      break;
    case this.ST_DL_ABORT: // await abort frame
      let status = (buffer[2] << 8) | buffer[3];
      console.log(`IndFileTransceiver.receive: ST_DL_ABORT, status ${this.toHex(buffer[2])}${this.toHex(buffer[3])}`);
      if (status === 0x8198) {
        this.terminateTransfer("Protocol error");
      }
      else {
        let ebcdic = buffer.slice(4, 84);
        let ascii = ebcdic.map(b => me.terminal.ebcdicToAscii[b]);
        let msg = ascii.map(b => String.fromCharCode(b)).join("");
        this.terminateTransfer(msg);
      }
      this.terminal.sendAID(this.terminal.AID_ENTER);
      break;
    default:
      console.log(`IndFileTransceiver.receive: Invalid state: ${this.state}`);
      this.terminateTransfer("Protocol error");
      break;
    }
  }

  receive(isText, fileName, successCallback, errorCallback, progressCallback) {
    const me = this;
    this.isText = isText;
    this.fileName = fileName;
    this.successCallback = successCallback;
    this.errorCallback = errorCallback;
    this.progressCallback = progressCallback;
    this.terminal = this.machine.getTerminal();
    this.fileData = [];
    this.messageHandler = status => {
      if (status.isTerminalWait === false) {
        me.processDownloadMsg();
      }
    };
    this.state = this.ST_DL_CMD_ACK;
    this.quadrant = 0;
    this.terminal.addTerminalStatusHandler(this.messageHandler);
  }

  sendUploadData(data) {
    let cksum = 0;
    for (let b of data) cksum ^= b;
    cksum = this.encode6BitTable[cksum & 0x3f];
    let seqNum = this.encode6BitTable[this.sendSeqNum];
    this.sendSeqNum = (this.sendSeqNum + 1) & 0x3f;
    let lenHigh = this.encode6BitTable[data.length >> 6];
    let lenLow  = this.encode6BitTable[data.length & 0x3f];
    this.lastMsg = [0x11, 0x40, 0xc2, this.MT_DATA_INDICATION, seqNum, cksum, lenHigh, lenLow].concat(data);
    this.terminal.sendAIDWithData(this.terminal.AID_ENTER, this.lastMsg);
  }

  processUploadMsg() {
    let me = this;
    let buffer = this.terminal.getBuffer();
    let msgType = buffer[0];
    let seqNum;
    switch (this.state) {
    case this.ST_UL_CMD_ACK: // await command acknowledgement frame
      seqNum = this.decode6BitTable[buffer[1]];
      if (this.debug)
        console.log(`IndFileTransceiver.transmit: state ST_UL_CMD_ACK, msgType ${this.toHex(msgType)}, seqNum ${seqNum} (expected ${this.recvSeqNum}), status ${this.toHex(buffer[2])}${this.toHex(buffer[3])}`);
      if (msgType === this.MT_CONTROL_CODE && seqNum === this.recvSeqNum && ((buffer[2] << 8) | buffer[3]) === 0x8181) {
        this.recvSeqNum = (this.recvSeqNum + 1) & 0x3f;
        this.state = this.ST_UL_DATA_REQ;
        this.terminal.sendAID(this.terminal.AID_ENTER);
      }
      break;
    case this.ST_UL_DATA_REQ: // await data indication frame
      if (this.debug)
        console.log(`IndFileTransceiver.transmit: state ST_UL_DATA_REQ, msgType ${this.toHex(msgType)}`);
      if (msgType === this.MT_DATA_REQUEST || msgType === this.MT_RETRANSMIT_REQUEST) {
        seqNum = this.decode6BitTable[buffer[3]];
        if (buffer[2] === this.MT_DATA_INDICATION && seqNum === this.recvSeqNum) {
          this.recvSeqNum = (this.recvSeqNum + 1) & 0x3f;
          if (msgType === this.MT_RETRANSMIT_REQUEST) {
            this.terminal.sendAIDWithData(this.terminal.AID_ENTER, this.lastMsg);
          }
          else if (this.fileDataIdx < this.fileData.length) {
            let data = [];
            while (this.fileDataIdx < this.fileData.length && data.length < this.MAX_UL_DATA_SIZE) {
              let b = this.fileData[this.fileDataIdx++];
              let quadrant = this.quadrant >= 0 ? this.quadrant : 0;
              if (this.isText && [0x5b, 0x5d, 0x7c].indexOf(b) >= 0) { // [, ], |
                let idx = [0x5b, 0x5d, 0x7c].indexOf(b);
                if (this.quadrant !== 0) {
                  data.push(this.quadrantKeys[0]);
                  this.quadrant = 0;
                }
                data.push([0xd5, 0xe5, 0x5d][idx]);
              }
              else {
                let idx = -1;
                for (let q = 0; q < 4; q++) {
                  idx = this.quadrants[quadrant].indexOf(b);
                  if (idx >= 0) break;
                  quadrant = (quadrant + 1) & 0x03;
                }
                if (idx >= 0) {
                  if (this.quadrant !== quadrant) {
                    data.push(this.quadrantKeys[quadrant]);
                    this.quadrant = quadrant;
                  }
                  data.push(this.quadrantAlphas[idx]);
                }
                else {
                  console.log(`IndFileTransceiver.transmit: no translation found for ${this.toHex(b)}`);
                }
              }
            }
            if (this.debug) this.logData("IndFileTransceiver.transmit: upload ", data);
            this.sendUploadData(data);
            if (typeof this.progressCallback === "function") this.progressCallback(this.fileDataIdx);
          }
          else { // end of file data
            this.sendUploadData([this.CR, this.EOF]);
            this.state = this.ST_UL_COMPLETION;
          }
        }
        else {
          console.log(`IndFileTransceiver.transmit: invalid data request, msgType ${this.toHex(buffer[2])}, seqNum ${seqNum} (expected ${this.recvSeqNum})`);
          this.state = this.ST_UL_ABORT;
          this.sendAbortAck(seqNum);
        }
      }
      else if (msgType === this.MT_CONTROL_CODE) {
        this.state = this.ST_UL_ABORT;
        this.processUploadMsg();
      }
      else {
        console.log(`IndFileTransceiver.transmit: invalid msgType ${this.toHex(msgType)} in state ${this.state}`);
        this.state = this.ST_UL_ABORT;
        this.sendAbortAck(0);
      }
      break;
    case this.ST_UL_COMPLETION: // await normal completion frame
      if (this.debug)
        console.log(`IndFileTransceiver.transmit: state ST_UL_COMPLETION, msgType ${this.toHex(msgType)}`);
      seqNum = this.decode6BitTable[buffer[1]];
      if (msgType === this.MT_CONTROL_CODE && seqNum === this.recvSeqNum && ((buffer[2] << 8) | buffer[3]) === 0x8189) {
        this.recvSeqNum = (this.recvSeqNum + 1) & 0x3f;
        this.terminal.removeTerminalStatusHandler(this.messageHandler);
        delete this.messageHandler;
        this.terminal.sendAID(this.terminal.AID_ENTER);
        if (typeof this.successCallback === "function") this.successCallback(this.fileData);
      }
      else {
        console.log(`IndFileTransceiver.transmit: invalid completion msg, msgType ${this.toHex(msgType)}, seqNum ${seqNum} (expected ${this.recvSeqNum}), status ${this.toHex(buffer[2])}${this.toHex(buffer[3])}`);
        this.state = this.ST_UL_ABORT;
        this.sendAbortAck(seqNum);
      }
      break;
    case this.ST_UL_ABORT: // await abort frame
      let status = (buffer[2] << 8) | buffer[3];
      console.log(`IndFileTransceiver.transmit: state ST_UL_ABORT, status ${this.toHex(buffer[2])}${this.toHex(buffer[3])}`);
      if (status === 0x8198) {
        this.terminateTransfer("Protocol error");
      }
      else {
        let ebcdic = buffer.slice(4, 84);
        let ascii = ebcdic.map(b => me.terminal.ebcdicToAscii[b]);
        let msg = ascii.map(b => String.fromCharCode(b)).join("");
        this.terminateTransfer(msg);
      }
      this.terminal.sendAID(this.terminal.AID_ENTER);
      break;
    default:
      console.log(`IndFileTransceiver.transmit: Invalid state: ${this.state}`);
      this.terminateTransfer("Protocol error");
      break;
    }
  }

  transmit(data, isText, fileName, successCallback, errorCallback, progressCallback) {
    const me = this;
    this.fileData = data;
    this.isText = isText;
    this.fileName = fileName;
    this.successCallback = successCallback;
    this.errorCallback = errorCallback;
    this.progressCallback = progressCallback;
    this.terminal = this.machine.getTerminal();
    this.messageHandler = status => {
      if (status.isTerminalWait === false) {
        me.processUploadMsg();
      }
    };
    this.state = this.ST_UL_CMD_ACK;
    this.fileDataIdx = 0;
    this.quadrant = -1;
    this.terminal.addTerminalStatusHandler(this.messageHandler);
  }
}
