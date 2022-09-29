/*--------------------------------------------------------------------------
**
**  Copyright (c) 2020, Kevin Jordan
**
**  kermit.js
**    This class implements the Kermit file transfer protocol.
**
**--------------------------------------------------------------------------
*/

class KermitTransceiver {

  constructor(machine) {

    this.debug     = false;

    this.SOH       = 0x01;
    this.LF        = 0x0a;
    this.CR        = 0x0d;
    this.Y         = 0x59;
    this.N         = 0x4e;

    this.ST_S_INIT = 0;
    this.ST_R_INIT = 1;
    this.ST_S_FILE = 2;
    this.ST_R_FILE = 3;
    this.ST_S_DATA = 4;
    this.ST_R_DATA = 5;
    this.ST_S_EOF  = 6;
    this.ST_S_EOT  = 7;
    this.ST_R_EOT  = 8;

    this.PKT_INIT  = 0x53; // "S" - send initiation packet
    this.PKT_FILE  = 0x46; // "F" - file header packet
    this.PKT_DATA  = 0x44; // "D" - file data packet
    this.PKT_EOF   = 0x5a; // "Z" - end of file packet
    this.PKT_EOT   = 0x42; // "B" - end of transaction packet
    this.PKT_ACK   = 0x59; // "Y" - acknowledgement packet
    this.PKT_NAK   = 0x4e; // "N" - negative acknowledgement packet
    this.PKT_ERR   = 0x45; // "E" - fatal error packet

    this.CKT_6     = 0x31; // "1" - 6-bit checksum
    this.CKT_12    = 0x32; // "2" - 12-bit checksum
    this.CKT_CRC   = 0x33; // "3" - CRC checksum

    this.MAXL      = 94;   // maximum packet length I can receive
    this.TIMO      = 5;    // maximum time peer should wait for packets from me
    this.NPAD      = 0;    // number of pad characters I want to receive
    this.PADC      = 0;    // pad character
    this.QCTL      = 0x23; // quote control character ("#")
    this.QRPT      = 0x7e; // quote run length ("~")
    this.EOLC      = 0x0d; // end of line character

    this.machine   = machine;
    this.init();
  }

  init() {
    this.maxl = this.MAXL;
    this.timo = this.TIMO;
    this.npad = this.NPAD;
    this.padc = this.PADC;
    this.qctl = this.QCTL;
    this.qbin = 0;
    this.qrpt = 0;
    this.checkType = this.CKT_6; // checksum type is 6-bit sum
    this.lastPkt = [];           // last packet sent
    this.sendEolc = this.EOLC;   // initial end of line character to send
    this.sendSeqNum = 0;         // initial packet sequence number to send
    this.recvSeqNum = 0;         // initial packet sequence number to receive
  }

  ctl(x) {
    return x ^ 0x40;
  }

  tochar(x) {
    return x + 32;
  }

  unchar(x) {
    return x - 32;
  }

  chksum(data, offset, limit) {
    let cs = 0;
    switch (this.checkType) {
    case this.CKT_6:
      while (offset < limit) {
        cs += data[offset++];
      }
      cs = (((cs & 0xc0) >> 6) + cs) & 0x3f;
      break;
    case this.CKT_12:
      while (offset < limit) {
        cs += data[offset++];
      }
      cs &= 0xfff;
      break;
    case this.CKT_CRC:
      //
      // Calculate the 16-bit CRC-CCITT of a byte array using lookup tables.
      //
      while (offset < limit) {
        let b = data[offset++] & 0xff;
        let q = (cs ^ b) & 0x0f;
        cs = (cs >> 4) ^ ((q * 0x1081) & 0xffff);
        q = (cs ^ (b >> 4)) & 0x0f;
        cs = (cs >> 4) ^ ((q * 0x1081) & 0xffff);
      }
      cs &= 0xffff;
      break;
    default:
      console.log(`Unsupported checksum type: ${this.checkType}`);
      break;
    }
    return cs;
  }

  logData(label, data) {
    let line = label;
    let prefix = "";
    for (let i = 0; i < label.length; i++) prefix += " ";
    for (let i = 0; i < data.length; i++) {
      let b = data[i];
      if (b >= 0x20 && b < 0x7f) {
        line += String.fromCharCode(b);
      }
      else if (b > 0x0f) {
        line += `<${b.toString(16)}>`;
      }
      else {
        line += `<0${b.toString(16)}>`;
      }
      if (line.length > 120) {
        console.log(line);
        line = prefix;
      }
    }
    if (line !== prefix) console.log(line);
  }

  invokeErrorCallback(msg) {
    this.disarmResendTimeout();
    this.disarmNakTimeout();
    this.machine.restoreState();
    if (this.debug) console.log(msg);
    if (typeof this.errorCallback === "function")
      this.errorCallback(msg);
  }

  resendLastPkt() {
    if (this.lastPkt && this.lastPkt.length > 0) {
      this.machine.send(new Uint8Array(this.lastPkt));
      if (this.debug) this.logData("SENT: ", this.lastPkt);
    }
    else {
      console.log("No packet to resend, NAK or timeout ignored");
    }
  }

  armResendTimeout() {
    if (this.timo < 1) return;
    const me = this;
    this.retryCount = 0;
    const armResend = () => {
      delete me.ackTimer;
      me.ackTimer = setTimeout(() => {
        console.log(`Timeout awaiting ACK (${me.retryCount + 1})`);
        if (++me.retryCount < 5) {
          me.resendLastPkt();
          armResend();
        }
        else if (typeof me.errorCallback === "function") {
          me.invokeErrorCallback("Transfer failed: max retries exceeded");
        }
      }, me.timo * 1000);
    };
    armResend();
  }

  disarmResendTimeout() {
    if (this.ackTimer) {
      clearTimeout(this.ackTimer);
      delete this.ackTimer;
    }
  }

  armNakTimeout() {
    if (this.timo < 1) return;
    const me = this;
    this.retryCount = 0;
    const armNak = () => {
      delete me.nakTimer;
      me.nakTimer = setTimeout(() => {
        console.log(`Timeout awaiting packet (${me.retryCount + 1})`);
        if (++me.retryCount < 5) {
          if (me.retryCount > 1) me.sendSeqNum = (me.sendSeqNum - 1) & 0x3f;
          me.sendPkt(me.PKT_NAK, []);
          armNak();
        }
        else if (typeof me.errorCallback === "function") {
          me.invokeErrorCallback("Transfer failed: max retries exceeded");
        }
      }, me.timo * 1000);
    };
    armNak();
  }

  disarmNakTimeout() {
    if (this.nakTimer) {
      clearTimeout(this.nakTimer);
      delete this.nakTimer;
    }
  }

  sendPkt(pktType, data) {
    this.lastPkt = [this.SOH, this.tochar(data.length + 3), this.tochar(this.sendSeqNum), pktType].concat(data);
    let cs = this.chksum(this.lastPkt, 1, this.lastPkt.length);
    switch (this.checkType) {
    case this.CKT_6:
      this.lastPkt.push(this.tochar(cs));
      break;
    case this.CKT_12:
      this.lastPkt.push(this.tochar(cs >> 6));
      this.lastPkt.push(this.tochar(cs & 0x3f));
      break;
    case this.CKT_CRC:
      this.lastPkt.push(this.tochar(cs >> 12));
      this.lastPkt.push(this.tochar((cs >> 6) & 0x3f));
      this.lastPkt.push(this.tochar(cs & 0x3f));
      break;
    default:
      console.log(`Unsupported checksum type: ${this.checkType}`);
      break;
    }
    this.lastPkt.push(this.sendEolc);
    this.sendSeqNum = (this.sendSeqNum + 1) & 0x3f;
    this.machine.send(new Uint8Array(this.lastPkt));
    if (this.debug) this.logData("SENT: ", this.lastPkt);
  }

  processInitPkt(data) {
    if (this.state !== this.ST_R_INIT) {
      console.log(`Protocol error: INIT received in state ${this.state}`);
      return;
    }
    this.disarmNakTimeout();
    let response = [];
    let i = 0;
    if (i < data.length) {
      this.maxl = this.unchar(data[i++]);
      response.push(this.tochar(this.MAXL));
    }
    if (i < data.length) {
      this.timo = this.unchar(data[i++]);
      response.push(this.tochar(this.TIMO));
    }
    if (i < data.length) {
      this.npad = this.unchar(data[i++]);
      response.push(this.tochar(this.NPAD));
    }
    if (i < data.length) {
      this.padc = this.ctl(data[i++]);
      response.push(this.ctl(this.PADC));
    }
    if (i < data.length) {
      this.sendEolc = this.unchar(data[i++]);
      response.push(this.tochar(this.EOLC));
    }
    if (i < data.length) {
      this.qctl = data[i++];
      response.push(this.qctl);
    }
    if (i < data.length) {
      let qbin = data[i++];
      response.push(this.Y);
    }
    if (i < data.length) {
      this.checkType = data[i++];
      this.checkType = this.CKT_6; // Force 6-bit checksum type
      response.push(this.checkType);
    }
    if (i < data.length) {
      this.qrpt = data[i++];
      response.push(this.qrpt);
    }
    if (this.debug) {
      console.log("Negotiated parameters:");
      console.log(`  maxl: ${this.maxl}`);
      console.log(`  timo: ${this.timo}`);
      console.log(`  npad: ${this.npad}`);
      console.log(`  padc: ${this.padc}`);
      console.log(`  eolc: ${this.sendEolc}`);
      console.log(`  qctl: ${String.fromCharCode(this.qctl)}`);
      console.log(`  qbin: ${String.fromCharCode(this.qbin)}`);
      console.log(`  chkt: ${String.fromCharCode(this.checkType)}`);
      console.log(`  qrpt: ${String.fromCharCode(this.qrpt)}`);
    }
    this.state = this.ST_R_FILE;
    this.sendPkt(this.PKT_ACK, response);
    this.armNakTimeout();
  }

  processFilePkt(data) {
    if (this.state !== this.ST_R_FILE) {
      console.log(`Protocol error: FILE received in state ${this.state}`);
      return;
    }
    this.disarmNakTimeout();
    this.state = this.ST_R_DATA;
    this.sendPkt(this.PKT_ACK, []);
    this.armNakTimeout();
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

  processDataPkt(data, isResent) {
    if (this.state !== this.ST_R_DATA) {
      console.log(`Protocol error: DATA received in state ${this.state}`);
      return;
    }
    this.disarmNakTimeout();
    if (isResent === false) this.fileData = this.fileData.concat(data);
    this.sendPkt(this.PKT_ACK, []);
    this.armNakTimeout();
    if (typeof this.progressCallback === "function") this.progressCallback(`${this.formatSize(this.fileData.length)} bytes`);
  }

  processEofPkt(data) {
    if (this.state !== this.ST_R_DATA) {
      console.log(`Protocol error: DATA received in state ${this.state}`);
      return;
    }
    this.disarmNakTimeout();
    this.state = this.ST_R_EOT;
    this.sendPkt(this.PKT_ACK, []);
    this.armNakTimeout();
    if (typeof this.progressCallback === "function") this.progressCallback(`${this.formatSize(this.fileData.length)} bytes (done)`);
  }

  processEotPkt(data) {
    if (this.state !== this.ST_R_EOT) {
      console.log(`Protocol error: EOT received in state ${this.state}`);
      return;
    }
    this.disarmNakTimeout();
    this.state = this.ST_R_INIT;
    this.sendPkt(this.PKT_ACK, []);
    this.machine.restoreState();
    if (typeof this.successCallback === "function") this.successCallback(this.fileData);
  }

  processAckPkt(data) {
    let i = 0;
    switch (this.state) {
    case this.ST_S_INIT:
      this.disarmResendTimeout();
      if (i < data.length)
        this.maxl = this.unchar(data[i++]);
      if (i < data.length)
        this.timo = this.unchar(data[i++]);
      if (i < data.length)
        this.npad = this.unchar(data[i++]);
      if (i < data.length)
        this.padc = this.ctl(data[i++]);
      if (i < data.length)
        this.sendEolc = this.unchar(data[i++]);
      this.qctl = (i < data.length) ? data[i++] : 0;
      if (i < data.length) {
        let qbin = data[i++];
        if (qbin !== this.Y && qbin !== this.N)
          this.qbin = qbin;
      }
      if (i < data.length)
        this.checkType = data[i++];
      this.qrpt = (i < data.length) ? data[i++] : 0;
      if (this.debug) {
        console.log("Negotiated parameters:");
        console.log(`  maxl: ${this.maxl}`);
        console.log(`  timo: ${this.timo}`);
        console.log(`  npad: ${this.npad}`);
        console.log(`  padc: ${this.padc}`);
        console.log(`  eolc: ${this.sendEolc}`);
        console.log(`  qctl: ${String.fromCharCode(this.qctl)}`);
        console.log(`  qbin: ${String.fromCharCode(this.qbin)}`);
        console.log(`  chkt: ${String.fromCharCode(this.checkType)}`);
        console.log(`  qrpt: ${String.fromCharCode(this.qrpt)}`);
      }
      let fileName = [];
      for (let i = 0; i < this.fileName.length; i++) {
        fileName.push(this.fileName.charCodeAt(i) & 0xff);
      }
      this.state = this.ST_S_FILE;
      this.sendPkt(this.PKT_FILE, fileName);
      this.armResendTimeout();
      break;
    case this.ST_S_FILE:
      this.state = this.ST_S_DATA;
      // fall through
    case this.ST_S_DATA:
      this.disarmResendTimeout();
      if (this.fileDataIndex >= this.fileData.length) {
        this.state = this.ST_S_EOF;
        this.sendPkt(this.PKT_EOF, []);
        this.armResendTimeout();
        if (typeof this.progressCallback === "function") this.progressCallback(1);
        return;
      }
      if (typeof this.progressCallback === "function") this.progressCallback(this.fileDataIndex / this.fileData.length);
      //
      // Encode file data to send
      //
      let encoded = [];
      const maxlen = this.maxl - 3;
      while (this.fileDataIndex < this.fileData.length && encoded.length < maxlen) {
        let ofdi = this.fileDataIndex;
        let olen = encoded.length;
        let b = this.fileData[this.fileDataIndex++];
        let rpt = 1;
        if (this.qrpt !== 0) {
          while (this.fileDataIndex < this.fileData.length
                 && b === this.fileData[this.fileDataIndex]
                 && rpt < 95) {
            rpt += 1;
            this.fileDataIndex += 1;
          }
          if (rpt < 3) {
            this.fileDataIndex -= rpt - 1;
            rpt = 1;
          }
        }
        if (rpt > 1) {
          encoded.push(this.qrpt);
          encoded.push(this.tochar(rpt));
        }
        let b7 = b & 0x7f;
        let b8 = b & 0x80;
        if (this.qbin !== 0 && b8 !== 0) {
          encoded.push(this.qbin);
          b = b7;
        }
        if (b7 < 0x20 || b7 === 0x7f) {
          encoded.push(this.qctl);
          b = this.ctl(b);
        }
        else if (b7 === this.qctl
                 || (this.qbin !== 0 && b7 === this.qbin)
                 || (this.qrpt !== 0 && b7 === this.qrpt)) {
          encoded.push(this.qctl);
        }
        encoded.push(b);
        if (encoded.length > maxlen) {
          this.fileDataIndex = ofdi;
          encoded = encoded.slice(0, olen);
          break;
        }
      }
      this.sendPkt(this.PKT_DATA, encoded);
      this.armResendTimeout();
      break;
    case this.ST_S_EOF:
      this.disarmResendTimeout();
      this.state = this.ST_S_EOT;
      this.sendPkt(this.PKT_EOT, []);
      this.armResendTimeout();
      break;
    case this.ST_S_EOT:
      this.disarmResendTimeout();
      this.machine.restoreState();
      if (typeof this.successCallback === "function") this.successCallback();
      break;
    default:
      console.log(`Protocol error: ACK received in state ${this.state}`);
      break;
    }
  }

  processErrPkt(data) {
    let s = data.map(b => {
      return String.fromCharCode(b);
    }).join("");
    this.invokeErrorCallback(s);
  }

  processRecvdPkt(pkt) {
    if (this.debug) this.logData("RCVD: ", pkt);
    let i = 0;
    /*
     *  Locate the start of the packet
     */
    while (i < pkt.length) {
      if (pkt[i++] === this.SOH) break;
    }
    if (pkt.length - i < 4) {
      if (this.debug) console.log("<SOH> not found");
      return;
    }
    /*
     *  Extract the packet length, sequence number, and packet type
     */
    let start = i;
    let pktLen  = this.unchar(pkt[i++]);
    let seqNum  = this.unchar(pkt[i++]);
    let pktType = pkt[i++];
    let limit = start + pktLen;
    if (limit > pkt.length) {
      console.log(`Invalid packet length: ${pktLen}`);
      return;
    }
    /*
     *  Compute and verify the checksum
     */
    let cs;
    switch (this.checkType) {
    case this.CKT_6:
      cs = this.chksum(pkt, start, limit);
      if (cs !== this.unchar(pkt[limit])) {
        console.log(`CKT_6 mismatch: ${cs} != ${this.unchar(pkt[limit])}`);
        return;
      }
      break;
    case this.CKT_12:
      let cs12 = (this.unchar(pkt[limit - 1]) << 6) | this.unchar(pkt[limit]);
      limit -= 1;
      cs = this.chksum(pkt, start, limit);
      if (cs !== cs12) {
        console.log(`CKT_12 mismatch: ${cs} != ${cs12}`);
        return;
      }
      break;
    case this.CKT_CRC:
      let crc = (((this.unchar(pkt[limit - 2]) & 0x0f) << 12) | (this.unchar(pkt[limit - 1]) << 6) | this.unchar(pkt[limit])) & 0xffff;
      limit -= 2;
      cs = this.chksum(pkt, start, limit);
      if (cs !== crc) {
        console.log(`CKT_CRC mismatch: ${cs} != ${crc}`);
        return;
      }
      break;
    default:
      console.log(`Unsupported checksum type: ${this.checkType}`);
      break;
    }
    /*
     *  Validate the sequence number
     */
    let isResent = false;
    if (seqNum !== this.recvSeqNum) {
      if (seqNum === ((this.recvSeqNum - 1) & 0x3f)) { // peer retransmitted last packet
        isResent = true;
      }
      else {
        console.log(`Sequence number mismatch: ${seqNum} != ${this.recvSeqNum}`);
        return;
      }
    }
    this.recvSeqNum = (seqNum + 1) & 0x3f;
    /*
     *  Decode the data, unless we're in INIT state negotiating
     *  protocol parameters.
     */
    let data = [];
    if (this.state > this.ST_R_INIT) {
      while (i < limit) {
        let b = pkt[i++];
        let rpt = 1;
        let b8  = 0;
        if (this.qrpt !== 0 && this.qrpt === b) {
          if (i + 1 >= limit) {
            console.log(`Packet truncated after "${String.fromCharCode(this.qrpt)}" (QRPT)`);
            return;
          }
          rpt = this.unchar(pkt[i++]);
          b = pkt[i++];
        }
        if (this.qbin !== 0 && this.qbin === b) {
          if (i >= limit) {
            console.log(`Packet truncated after "${String.fromCharCode(this.qbin)}" (QBIN)`);
            return;
          }
          b = pkt[i++];
          b8 = 0x80;
        }
        if (this.qctl === b) {
          if (i >= limit) {
            console.log(`Packet truncated after "${String.fromCharCode(this.qctl)}" (QCTL)`);
            return;
          }
          b = pkt[i++];
          let b7 = b & 0x7f;
          if (b7 > 62 && b7 < 96) b = this.ctl(b);
        }
        b |= b8;
        while (rpt-- > 0) data.push(b);
      }
    }
    else { // don't decode data in INIT state
      data = pkt.slice(i, limit);
    }
    if (isResent && pktType !== this.PKT_NAK) {
      this.sendSeqNum = (this.sendSeqNum - 1) & 0x3f;
    }
    switch (pktType) {
    case this.PKT_INIT:
      this.processInitPkt(data);
      break;
    case this.PKT_FILE:
      this.processFilePkt(data);
      break;
    case this.PKT_DATA:
      this.processDataPkt(data, isResent);
      break;
    case this.PKT_EOF:
      this.processEofPkt(data);
      break;
    case this.PKT_EOT:
      this.processEotPkt(data);
      break;
    case this.PKT_ACK:
      this.processAckPkt(data);
      break;
    case this.PKT_NAK:
      this.disarmResendTimeout();
      this.resendLastPkt();
      this.armResendTimeout();
      break;
    case this.PKT_ERR:
      this.processErrPkt(data);
      break;
    default:
      console.log(`Unrecognized packet type:${pktType}`);
      break;
    }
  }

  receive(isText, fileName, successCallback, errorCallback, progressCallback) {
    const me = this;
    this.fileName = fileName;
    if (isText) {
      this.successCallback = data => {
        if (typeof successCallback === "function") {
          successCallback(me.denormalizeText(data));
        }
      };
    }
    else {
      this.successCallback = successCallback;
    }
    this.errorCallback = errorCallback;
    this.progressCallback = progressCallback;
    this.fileData = [];
    this.state = this.ST_R_INIT;
    let receivedPkt = [];
    this.machine.saveState();
    this.machine.receivedDataHandler = data => {
      if (Array.isArray(data)) {
        receivedPkt = receivedPkt.concat(data);
      }
      else if (typeof data === "string") {
        for (let i = 0; i < data.length; i++) receivedPkt.push(data.charCodeAt(i));
      }
      else {
        for (let b of data) receivedPkt.push(b);
      }
      let last = receivedPkt.length - 1;
      if (last >= 0 && (receivedPkt[last] === this.CR || receivedPkt[last] === this.LF)) {
        me.processRecvdPkt(receivedPkt);
        receivedPkt = [];
      }
    };
  }

  denormalizeText(data) {
    if (!Array.isArray(data)) data = Array.from(data);
    let i = data.indexOf(this.CR);
    while (i !== -1) {
      if (i >= 0 && i + 1 < data.length && data[i + 1] === this.LF) {
        data.splice(i, 1);
      }
      i = data.indexOf(this.CR, i + 1);
    }
    return data;
  }

  transmit(data, isText, fileName, successCallback, errorCallback, progressCallback) {
    const me = this;
    this.fileData = isText ? this.normalizeText(data) : data;
    this.fileDataIndex = 0;
    this.fileName = fileName;
    this.successCallback = successCallback;
    this.errorCallback = errorCallback;
    this.progressCallback = progressCallback;
    this.state = this.ST_S_INIT;
    let receivedPkt = [];
    this.machine.saveState();
    this.machine.receivedDataHandler = data => {
      if (Array.isArray(data)) {
        receivedPkt = receivedPkt.concat(data);
      }
      else if (typeof data === "string") {
        for (let i = 0; i < data.length; i++) receivedPkt.push(data.charCodeAt(i));
      }
      else {
        for (let b of data) receivedPkt.push(b);
      }
      let last = receivedPkt.length - 1;
      if (last >= 0 && (receivedPkt[last] === this.CR || receivedPkt[last] === this.LF)) {
        me.processRecvdPkt(receivedPkt);
        receivedPkt = [];
      }
    };
    this.sendPkt(this.PKT_INIT, [
      this.tochar(this.MAXL), 
      this.tochar(this.TIMO),
      this.tochar(this.NPAD),
      this.ctl(this.PADC),
      this.tochar(this.EOLC),
      this.QCTL,
      this.Y,
      this.CKT_6,
      this.QRPT
    ]);
    this.armResendTimeout();
  }

  normalizeText(data) {
    if (!Array.isArray(data)) data = Array.from(data);
    let i = data.indexOf(this.LF);
    while (i !== -1) {
      if (i > 0 && data[i - 1] !== this.CR) {
        data.splice(i, 0, this.CR);
        i = data.indexOf(this.LF, i + 2);
      }
      else {
        i = data.indexOf(this.LF, i + 1);
      }
    }
    return data;
  }
}
