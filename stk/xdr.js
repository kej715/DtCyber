/*
 *  This class provides methods to de/serialize data in conformance
 *  with the Internet XDR specification as defined by RFC1832:
 *
 *          https://tools.ietf.org/html/rfc1832
 *
 */

class XDR {

  constructor() {
    this.in   = 0;
    this.out  = 0;
    this.data = new Uint8Array(256);
  }

  getData() {
    return this.data.slice(0, this.in);
  }

  setData(data) {
    this.data = new Uint8Array(data);
    this.in   = this.data.byteLength;
    this.out  = 0;
  }

  getLength() {
    return this.in;
  }

  extendData(additionalSize) {
    let newData = new Uint8Array(this.data.byteLength + additionalSize);
    newData.set(this.data);
    this.data = newData;
  }

  appendUnsignedInt(value) {
    if (this.in + 4 > this.data.byteLength) this.extendData(256);
    this.data[this.in++] = (value >> 24) & 0xff;
    this.data[this.in++] = (value >> 16) & 0xff;
    this.data[this.in++] = (value >>  8) & 0xff;
    this.data[this.in++] = value & 0xff;
  }

  extractUnsignedInt() {
    let value = 0;
    for (let i = 0; i < 4; i++) {
      value *= 256;
      value += this.data[this.out++];
    }
    return value;
  }

  appendInt(value) {
    this.appendUnsignedInt(value);
  }

  extractInt() {
    let value = this.extractUnsignedInt();
    if (value > 0x7fffffff) {
      value = (value & 0x7fffffff) - 0x80000000;
    }
    return value;
  }

  appendEnum(value) {
    this.appendInt(value);
  }

  extractEnum() {
    return this.extractInt();
  }

  appendBoolean(value) {
    this.appendUnsignedInt(value ? 1 : 0);
  }

  extractBoolean() {
    return this.extractUnsignedInt() !== 0;
  }

  appendFixedOpaque(value) {
    let roundedLength = Math.floor((value.length + 3) / 4) * 4;
    if (this.in + roundedLength > this.data.byteLength) this.extendData(roundedLength);
    this.data.set(value, this.in);
    let limit = this.in + roundedLength;
    this.in += value.length;
    while (this.in < limit) this.data[this.in++] = 0;
  }

  extractFixedOpaque(length) {
    let roundedLength = Math.floor((length + 3) / 4) * 4;
    let value = this.data.slice(this.out, this.out + length);
    this.out += roundedLength;
    return value;
  }

  appendVariableOpaque(value) {
    this.appendUnsignedInt(value.length);
    this.appendFixedOpaque(value);
  }

  extractVariableOpaque() {
    let length = this.extractUnsignedInt();
    return this.extractFixedOpaque(length);
  }

  appendString(value) {
    let bytes = [];
    for (let i = 0; i < value.length; i++) bytes.push(value.charCodeAt(i) & 0xff);
    bytes.push(0);
    this.appendVariableOpaque(bytes);
  }

  extractString() {
    let value = this.extractVariableOpaque();
    let s = "";
    for (let code of value) {
      if (code === 0) break;
      s += String.fromCharCode(code);
    }
    return s;
  }

  extractRemainder() {
    return this.data.slice(this.out);
  }
}

module.exports = XDR;
