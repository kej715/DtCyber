/*--------------------------------------------------------------------------
**
**  Copyright (c) 2022, Kevin Jordan
**
**  pterm-classic.js
**    This class implements a PLATO classic terminal emulator.
**
**--------------------------------------------------------------------------
*/

class PTermClassic {

  constructor() {

    this.CHARSET_M0  = 0;
    this.CHARSET_M1  = 1;
    this.CHARSET_M2  = 2;
    this.CHARSET_M3  = 3;
    this.CHARSET_M4  = 4;
    this.CHARSET_M5  = 5;
    this.CHARSET_M6  = 6;
    this.CHARSET_M7  = 7;

    this.CHAR_SIZE_0 = 0;
    this.CHAR_SIZE_2 = 2;

    this.CHAR_STATE_NORMAL = 0;
    this.CHAR_STATE_ESCAPE = 1;

    this.DIRECTION_FORWARD = 0;
    this.DIRECTION_REVERSE = 1;

    this.MODE_INVERSE = 0;
    this.MODE_REWRITE = 1;
    this.MODE_ERASE   = 2;
    this.MODE_WRITE   = 3;

    this.MODE_POINT          = 0;
    this.MODE_LINE           = 1;
    this.MODE_WRITE_MEMORY   = 2;
    this.MODE_PLOT_CHARS     = 3;
    this.MODE_BLOCK_ERASE    = 4;
    this.MODE_PROGRAM_ORG5   = 5;
    this.MODE_PROGRAM_ORG6   = 6;
    this.MODE_PROGRAM_ORG7   = 7;

    this.NOP_MASK        = 0o1700000; // mask off plato word except nop bits
    this.NOP_MASK_DATA   =   0o77000; // mask bits 9-14, gives 6 bits for special data in a NOP code; lower 9 bits still available for meta data
    this.NOP_SET_STATION =   0o42000; // set station number
    this.NOP_META_START  =   0o43000; // start streaming PLATO metadata - lowest 6 bits is a character
    this.NOP_META_STREAM =   0o44000; // stream PLATO metadata
    this.NOP_META_STOP   =   0o45000; // stop streaming PLATO metadata
    this.NOP_FONT_TYPE   =   0o50000; // font type
    this.NOP_FONT_SIZE   =   0o51000; // font size
    this.NOP_FONT_FLAG   =   0o52000; // font flags
    this.NOP_FONT_INFO   =   0o53000; // get last font character width/height
    this.NOP_OS_INFO     =   0o54000; // get operating system type, returns 1=mac, 2=windows, 3=linux

    this.ORIENTATION_HORIZONTAL = 0;
    this.ORIENTATION_VERTICAL   = 1;

    this.TERMINAL_TYPE = 10;

    this.M5_ORIGIN     = 0x2300; // pointer in ram to mode 5 program
    this.M6_ORIGIN     = 0x2302; // pointer in ram to mode 5 program
    this.M7_ORIGIN     = 0x2304; // pointer in ram to mode 5 program
    this.C2_ORIGIN     = 0x2306; // pointer in ram to M2 characters
    this.C3_ORIGIN     = 0x2308; // pointer in ram to M3 characters
    this.C4_ORIGIN     = 0x230a; // pointer in ram to M4 characters
    this.C5_ORIGIN     = 0x230c; // pointer in ram to M5 characters
    this.C6_ORIGIN     = 0x230e; // pointer in ram to M6 characters
    this.C7_ORIGIN     = 0x2310; // pointer in ram to M7 characters
    this.M2_ADDRESS    = 0x2340; // default start address in ram of char set M2
    this.M3_ADDRESS    = 0x2740; // default start address in ram of char set M3

    this.bgndColor     = "#000000"; // black
    this.canvas        = null;
    this.charArray     = [];
    this.charState     = this.CHAR_STATE_NORMAL;
    this.context       = null;
    this.fgndColor     = "#ffa500"; // orange
    this.fontHeight    = 16;
    this.fontWidth     = 8;
    this.inverse       = false;
    this.isDebug       = false;
    this.lastX         = 0;  // last computed X coordinate
    this.lastY         = 0;  // last computed Y coordinate
    this.receivedBytes = [];
    this.wc            = 0;
    this.X             = 0;  // current X coordinate
    this.Y             = 0;  // current Y coordinate

    this.bgndColor         = "#000000"; // black
    this.charset           = this.CHARSET_M0;
    this.fontHeight        = 16;
    this.charSize          = this.CHAR_SIZE_0;
    this.charState         = this.CHAR_STATE_NORMAL;
    this.fontWidth         = 8;
    this.direction         = this.DIRECTION_FORWARD;
    this.renderMode        = this.MODE_PLOT_CHARS;
    this.fgndColor         = "#ffa500"; // orange
    this.graphicsMode      = this.MODE_WRITE;
    this.lastX             = 0;
    this.lastY             = 0;
    this.margin            = [0, 0];
    this.orientation       = this.ORIENTATION_HORIZONTAL;
    this.receivedBytes     = [];
    this.touchPanelEnabled = false;
    this.wc                = 0;
    this.X                 = 0;
    this.Y                 = 496;

    this.ram = new Uint8Array(0x10000);
    this.ram.fill(0);
    this.ram[this.C2_ORIGIN]     =  this.M2_ADDRESS & 0xff;
    this.ram[this.C2_ORIGIN + 1] = (this.M2_ADDRESS >> 8) & 0xff;
    this.ram[this.C3_ORIGIN]     =  this.M3_ADDRESS & 0xff;
    this.ram[this.C3_ORIGIN + 1] = (this.M3_ADDRESS >> 8) & 0xff;
    this.c2origin = this.M2_ADDRESS;
    this.c3origin = this.M3_ADDRESS;
    this.ramAddress = 0;

    this.alternateCharset = [];
    for (let i = 0; i < 128; i++) {
      let char = [];
      for (let j = 0; j < 8; j++) {
        char.push(0);
      }
      this.alternateCharset.push(char);
    }

    this.cdcToAscii = [
      0x3a,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f, // 000 - 017
      0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x30,0x31,0x32,0x33,0x34, // 020 - 037
      0x35,0x36,0x37,0x38,0x39,0x2b,0x2d,0x2a,0x2f,0x28,0x29,0x24,0x3d,0x20,0x2c,0x2e, // 040 - 057
      0x23,0x5b,0x5d,0x25,0x22,0x5f,0x21,0x26,0x27,0x3f,0x3c,0x3d,0x40,0x5c,0x5e,0x3b  // 060 - 077
    ];

    this.M0charset = [
      0x3a,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f, // :abcdefghijklmno
      0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x30,0x31,0x32,0x33,0x34, // pqrstuvwxyz01234
      0x35,0x36,0x37,0x38,0x39,0x2b,0x2d,0x2a,0x2f,0x28,0x29,0x24,0x3d,0x20,0x2c,0x2e, // 56789+-*/()$= ,.
      0x80,0x5b,0x5d,0x25,0x81,0x82,0x27,0x22,0x21,0x3b,0x3c,0x3d,0x5f,0x3f,0xbb,0x20  // <div>[]%<mul><left>'"!;<>_?<arrow> 
    ];

    this.M1charset = [
      0x23,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f, // #ABCDEFGHIJKLMNO
      0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x84,0x85,0x5e,0x87,0x60, // PQRSTUVWXYZ<tilde><dieresis>^<acute>`
      0x89,0x8a,0x8b,0x8c,0x7e,0x8d,0x8e,0x8f,0x90,0x7b,0x7d,0x26,0x91,0x20,0x7c,0x92, // <up><rt><dn><lf>~<sig><delta><un><inter>{}&<neq> |<deg>
      0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9e,0x9f,0x9d,0x40,0x5c,0x20  // <eqv><alpha><beta><delta><lambda><mu><pi><rho><sigma><omega><le><ge><theta>@\ 
    ];

    //
    // This table is indexed by keyboard key. Each entry provides a sequence
    // of codes to be sent upline.
    //
    this.platoKeyMap = [
      //
      // <NUL> - <US>
      //
      [],                    [0o022],
      [0o030],               [0o033],
      [0o031],               [0o027],
      [0o064],               [0o013],
      [0o025],               [],
      [],                    [],
      [0o035],               [0o024],
      [0o026],               [],
      [0o020],               [0o034],
      [0o023],               [0o032],
      [0o062],               [],
      [],                    [],
      [0o012],               [0o021],
      [],                    [],
      [],                    [],
      [],                    [],
      //
      // <SP> - ?
      [0o100],               [0o176],
      [0o177],               [0o074, 0o044],
      [0o044],               [0o045],
      [0o074, 0o016],        [0o047],
      [0o051],               [0o173],
      [0o050],               [0o016],
      [0o137],               [0o017],
      [0o136],               [0o135],
      [0o000],               [0o001],
      [0o002],               [0o003],
      [0o004],               [0o005],
      [0o006],               [0o007],
      [0o010],               [0o011],
      [0o174],               [0o134],
      [0o040],               [0o133],
      [0o041],               [0o175],
      //
      // @ - _
      //
      [0o074, 0o005],        [0o141],
      [0o142],               [0o143],
      [0o144],               [0o145],
      [0o146],               [0o147],
      [0o150],               [0o151],
      [0o152],               [0o153],
      [0o154],               [0o155],
      [0o156],               [0o157],
      [0o160],               [0o161],
      [0o162],               [0o163],
      [0o164],               [0o165],
      [0o166],               [0o167],
      [0o170],               [0o171],
      [0o172],               [0o042],
      [0o074, 0o135],        [0o043],
      [0o100, 0o074, 0o130], [0o046],
      //
      // ` - <DEL>
      //
      [0o100, 0o074, 0o121], [0o101],
      [0o102],               [0o103],
      [0o104],               [0o105],
      [0o106],               [0o107],
      [0o110],               [0o111],
      [0o112],               [0o113],
      [0o114],               [0o115],
      [0o116],               [0o117],
      [0o120],               [0o121],
      [0o122],               [0o123],
      [0o124],               [0o125],
      [0o126],               [0o127],
      [0o130],               [0o131],
      [0o132],               [0o074, 0o042],
      [0o074, 0o151],        [0o074, 0o043],
      [0o100, 0o074, 0o116], []
    ];

    this.charsets  = [this.M0charset, this.M1charset];
  }

  toHex(value) {
    if (Array.isArray(value) || typeof value === "object") {
      let result = [];
      for (let i = 0; i < value.length; i++) {
        let byte = value[i];
        result.push((byte < 16) ? "0" + byte.toString(16) : byte.toString(16));
      }
      return result;
    }
    else {
      let result = value.toString(16);
      if ((result.length & 1) !== 0) result = "0" + result;
      return result;
    }
  }

  toOctal(value, digits) {
    let s = "";
    while (value !== 0) {
      s = `${(value & 7).toString(8)}${s}`;
      value >>= 3;
    }
    while (s.length < digits) s = `0${s}`;
    return s;
  }

  debug(message, bytes) {
    if (this.isDebug) {
      if (typeof bytes !== "undefined") {
        if (Array.isArray(bytes))
          console.log(`${message}: [${this.toHex(bytes).join(" ")}]`);
        else
          console.log(`${message}: [${this.toHex(bytes)}]`);
      }
      else {
        console.log(message);
      }
    }
  }

  setDebug(db) {
    this.isDebug = db;
  }

  setUplineDataSender(callback) {
    this.uplineDataSender = callback;
  }

  setFont() {
    this.context.font = "normal 16px PlatoAscii";
  }

  setFontId(id) {
    this.debug(`Set font ID : ${id}`);
  }

  setFontSize(size) {
    this.debug(`Set font size : ${size}`);
  }

  setFontFlags(flags) {
    this.debug(`Set font flags : ${flags.toString(8)}`);
  }

  processKeyboardEvent(keyStr, shiftKey, ctrlKey, altKey) {
    let bytes = [];
    //
    // Handle non-special keys
    //
    if (keyStr.length < 2) { // non-special key
      let c = keyStr.charCodeAt(0);
      if (ctrlKey) {
        if ((c >= 0x41 && c <= 0x5a)
            || (c >= 0x61 && c <= 0x7a)) {
          bytes = this.platoKeyMap[c & 0x1f];
          if (shiftKey && bytes.length === 1)
            bytes[0] |= 0o40;
        }
      }
      else if (c < this.platoKeyMap.length) {
        bytes = this.platoKeyMap[c];
      }
    }
    //
    // Handle special keys
    //
    else {

      switch (keyStr) {
      case "Backspace":
      case "Delete":
        bytes = [0o023];
        break;
      case "Enter":
        bytes = [0o026];
        break;
      case "Escape":
        break;
      case "Tab":
        break;
      case "ArrowUp":
        bytes = [0o127];
        break;
      case "ArrowDown":
        bytes = [0o130];
        break;
      case "ArrowRight":
        bytes = [0o104];
        break;
      case "ArrowLeft":
        bytes = [0o101];
        break;
      case "F1":
        break;
      case "F2":
        break;
      case "F3":
        break;
      case "F4":
        break;
      case "F5":
        break;
      case "F6":
        break;
      case "F7":
        break;
      case "F8":
        break;
      case "F9":
        break;
      case "F10":
        break;
      default: // ignore the key
        break;
      }
    }
    this.send(bytes);
  }

  send(bytes) {
    if (bytes.length > 0 && this.uplineDataSender) {
      let buffer = new Uint8Array(bytes.length * 2);
      let bi = 0;
      for (let i = 0; i < bytes.length; i++) {
        let b = bytes[i];
        buffer[bi++] = b >> 7;
        buffer[bi++] = (b & 0x7f) | 0x80;
      }
      if (this.isDebug) {
        let ary = [];
        for (let b of buffer) ary.push(b);
        this.debug("send", ary);
      }
      this.uplineDataSender(buffer);
    }
  }

  createScreen(container) {

    const me = this;

    this.canvas = document.createElement("canvas");
    this.canvas.width  = 512;
    this.canvas.height = 512;
    this.canvas.style.border = "1px solid black";
    this.canvas.style.cursor = "default";
    this.context = this.canvas.getContext("2d");
    this.reset();
    this.canvas.setAttribute("tabindex", 0);
    this.canvas.setAttribute("contenteditable", "true");

    this.canvas.addEventListener("click", evt => {
      me.touchPanelEventHandler.call(me, evt);
    });

    this.canvas.addEventListener("mouseover", () => {
      me.canvas.focus();
    });

    this.canvas.addEventListener("keydown", evt => {
      evt.preventDefault();
      me.processKeyboardEvent(evt.key, evt.shiftKey, evt.ctrlKey, evt.altKey);
    });
  }

  getCanvas() {
    return this.canvas;
  }

  reset() {
    this.bgndColor         = "#000000"; // black
    this.charset           = this.CHARSET_M0;
    this.fontHeight        = 16;
    this.charSize          = this.CHAR_SIZE_0;
    this.charState         = this.CHAR_STATE_NORMAL;
    this.fontWidth         = 8;
    this.direction         = this.DIRECTION_FORWARD;
    this.renderMode        = this.MODE_PLOT_CHARS;
    this.fgndColor         = "#ffa500"; // orange
    this.graphicsMode      = this.MODE_WRITE;
    this.lastX             = 0;
    this.lastY             = 0;
    this.margin            = [0, 0];
    this.orientation       = this.ORIENTATION_HORIZONTAL;
    this.receivedBytes     = [];
    this.touchPanelEnabled = false;
    this.wc                = 0;
    this.X                 = 0;
    this.Y                 = 496;

    this.context.fillStyle = this.bgndColor;
    this.context.fillRect(0, 0, 512, 512);
    this.setFont();

    this.ram.fill(0);
    this.ram[this.C2_ORIGIN]     =  this.M2_ADDRESS & 0xff;
    this.ram[this.C2_ORIGIN + 1] = (this.M2_ADDRESS >> 8) & 0xff;
    this.ram[this.C3_ORIGIN]     =  this.M3_ADDRESS & 0xff;
    this.ram[this.C3_ORIGIN + 1] = (this.M3_ADDRESS >> 8) & 0xff;
    this.c2origin = this.M2_ADDRESS;
    this.c3origin = this.M3_ADDRESS;
    this.ramAddress = 0;

    this.alternateCharset = [];
    for (let i = 0; i < 128; i++) {
      let char = [];
      for (let j = 0; j < 8; j++) {
        char.push(0);
      }
      this.alternateCharset.push(char);
    }
  }

  drawStdChar(c6, x, y) {
    const chCode = this.charsets[this.charset][c6];
    this.context.textBaseline = "bottom";
    if (this.graphicsMode === this.MODE_WRITE) {
      this.context.fillStyle = this.fgndColor;
      this.context.fillText(String.fromCharCode(chCode), x, 511 - y);
      if (this.isDebug) {
        let s = (chCode >= 0x20 && chCode < 0x7f) ? String.fromCharCode(chCode) : ".";
        this.debug(`drawStdChar WRITE ${s} <${this.toHex(chCode)}> at (${x},${y}), charset: M${this.charset}, fgnd: ${this.fgndColor}`);
      }
    }
    else if (this.graphicsMode === this.MODE_ERASE) {
      this.context.fillStyle = this.bgndColor;
      this.context.fillText(String.fromCharCode(chCode), x, 511 - y);
      if (this.isDebug) {
        let s = (chCode >= 0x20 && chCode < 0x7f) ? String.fromCharCode(chCode) : ".";
        this.debug(`drawStdChar ERASE ${s} <${this.toHex(chCode)}> at (${x},${y}), charset: M${this.charset}, bgnd: ${this.bgndColor}`);
      }
    }
    else if (this.graphicsMode === this.MODE_REWRITE) {
      this.context.fillStyle = this.bgndColor;
      this.context.fillRect(x, 511 - y - 15, this.fontWidth, this.fontHeight);
      this.context.fillStyle = this.fgndColor;
      this.context.fillText(String.fromCharCode(chCode), x, 511 - y);
      if (this.isDebug) {
        let s = (chCode >= 0x20 && chCode < 0x7f) ? String.fromCharCode(chCode) : ".";
        this.debug(`drawStdChar REWRITE ${s} <${this.toHex(chCode)}> at (${x},${y}), charset: M${this.charset}, fgnd: ${this.fgndColor}, bgnd: ${this.bgndColor}`);
      }
    }
    else { // MODE_INVERSE
      this.context.fillStyle = this.fgndColor;
      this.context.fillRect(x, 511 - y - 15, this.fontWidth, this.fontHeight);
      this.context.fillStyle = this.bgndColor;
      this.context.fillText(String.fromCharCode(chCode), x, 511 - y);
      if (this.isDebug) {
        let s = (chCode >= 0x20 && chCode < 0x7f) ? String.fromCharCode(chCode) : ".";
        this.debug(`drawStdChar INVERSE ${s} <${this.toHex(chCode)}> at (${x},${y}), charset: M${this.charset}, fgnd: ${this.fgndColor}, bgnd: ${this.bgndColor}`);
      }
    }
  }

  drawAltChar(chCode, x, y) {
    if (this.charset > this.CHARSET_M2) chCode += 64;
    let bitmap = this.alternateCharset[chCode];
    y = 511 - (y + 15);
    let charBlock = this.context.getImageData(x, y, 8, 16);
    if (this.graphicsMode === this.MODE_WRITE) {
      let pixelData = [parseInt(this.fgndColor.substring(1, 3), 16),
                       parseInt(this.fgndColor.substring(3, 5), 16),
                       parseInt(this.fgndColor.substring(5, 7), 16),
                       255];
      let charBlockOffset = 0;
      let bitmapMask = 0x8000;
      for (let row = 0; row < 16; row++) {
        for (let col = 0; col < 8; col++) {
          if (bitmap[col] & bitmapMask) {
            charBlock.data.set(pixelData, charBlockOffset);
          }
          charBlockOffset += 4;
        }
        bitmapMask >>= 1;
      }
      this.debug(`drawAltChar WRITE <${this.toHex(chCode)}> at (${x},${y}), "charset: M${this.charset}, size: ${this.charSize}, fgnd: ${this.fgndColor}, bgnd: ${this.bgndColor}`);
    }
    else if (this.graphicsMode === this.MODE_ERASE) {
      let pixelData = [parseInt(this.bgndColor.substring(1, 3), 16),
                       parseInt(this.bgndColor.substring(3, 5), 16),
                       parseInt(this.bgndColor.substring(5, 7), 16),
                       255];
      let charBlockOffset = 0;
      let bitmapMask = 0x8000;
      for (let row = 0; row < 16; row++) {
        for (let col = 0; col < 8; col++) {
          if (bitmap[col] & bitmapMask) {
            charBlock.data.set(pixelData, charBlockOffset);
          }
          charBlockOffset += 4;
        }
        bitmapMask >>= 1;
      }
      this.debug(`drawAltChar ERASE <${this.toHex(chCode)}> at (${x},${y}), "charset: M${this.charset}, size: ${this.charSize}, fgnd: ${this.fgndColor}, bgnd: ${this.bgndColor}`);
    }
    else if (this.graphicsMode === this.MODE_REWRITE) {
      let fgndPixelData = [parseInt(this.fgndColor.substring(1, 3), 16),
                           parseInt(this.fgndColor.substring(3, 5), 16),
                           parseInt(this.fgndColor.substring(5, 7), 16),
                           255];
      let bgndPixelData = [parseInt(this.bgndColor.substring(1, 3), 16),
                           parseInt(this.bgndColor.substring(3, 5), 16),
                           parseInt(this.bgndColor.substring(5, 7), 16),
                           255];
      let charBlockOffset = 0;
      let bitmapMask = 0x8000;
      for (let row = 0; row < 16; row++) {
        for (let col = 0; col < 8; col++) {
          if (bitmap[col] & bitmapMask) {
            charBlock.data.set(fgndPixelData, charBlockOffset);
          }
          else {
            charBlock.data.set(bgndPixelData, charBlockOffset);
          }
          charBlockOffset += 4;
        }
        bitmapMask >>= 1;
      }
      this.debug(`drawAltChar REWRITE <${this.toHex(chCode)}> at (${x},${y}), "charset: M${this.charset}, size: ${this.charSize}, fgnd: ${this.fgndColor}, bgnd: ${this.bgndColor}`);
    }
    else { // MODE_INVERSE
      let bgndPixelData = [parseInt(this.fgndColor.substring(1, 3), 16),
                           parseInt(this.fgndColor.substring(3, 5), 16),
                           parseInt(this.fgndColor.substring(5, 7), 16),
                           255];
      let fgndPixelData = [parseInt(this.bgndColor.substring(1, 3), 16),
                           parseInt(this.bgndColor.substring(3, 5), 16),
                           parseInt(this.bgndColor.substring(5, 7), 16),
                           255];
      let charBlockOffset = 0;
      let bitmapMask = 0x8000;
      for (let row = 0; row < 16; row++) {
        for (let col = 0; col < 8; col++) {
          if (bitmap[col] & bitmapMask) {
            charBlock.data.set(fgndPixelData, charBlockOffset);
          }
          else {
            charBlock.data.set(bgndPixelData, charBlockOffset);
          }
          charBlockOffset += 4;
        }
        bitmapMask >>= 1;
      }
      this.debug(`drawAltChar INVERSE <${this.toHex(chCode)}> at (${x},${y}), "charset: M${this.charset}, size: ${this.charSize}, fgnd: ${this.fgndColor}, bgnd: ${this.bgndColor}`);
    }
    this.context.putImageData(charBlock, x, y);
  }

  drawChar(chCode, x, y) {
    if (this.charset < this.CHARSET_M2) {
      this.drawStdChar(chCode, x, y);
    }
    else if (this.charset < this.CHARSET_M4) {
      this.drawAltChar(chCode, x, y);
    }
    this.X = (x + this.fontWidth) & 0x1ff; // TODO: accommodate direction and orientation
    this.Y = y;
  }

  drawPoint(x, y) {
    if (this.graphicsMode === this.MODE_WRITE || this.graphicsMode === this.MODE_REWRITE) {
      this.context.fillStyle = this.fgndColor;
      this.debug(`draw point RE/WRITE: (${x},${y}), fgnd: ${this.fgndColor}`);
    }
    else { // MODE_ERASE or MODE_INVERSE
      this.context.fillStyle = this.bgndColor;
      this.debug(`draw point ERASE/INVERSE: (${x},${y}), bgnd: ${this.bgndColor}`);
    }
    this.context.fillRect(x, 511 - y, 1, 1);
    this.X = x;
    this.Y = y;
  }

  /*
   * Draw a line a point at a time to avoid the antialiasing that's
   * implemented by the lineTo function of the canvas. The antialiasing
   * makes it impossible to find the edges of areas while executing
   * paint commands.
   */
  drawLine(x1, y1, x2, y2) {
    y1 = 511 - y1;
    y2 = 511 - y2;
    if (this.graphicsMode === this.MODE_WRITE || this.graphicsMode === this.MODE_REWRITE) {
      this.context.fillStyle = this.fgndColor;
      this.debug(`draw line RE/WRITE: (${x1},${y1}) -> (${x2},${y2}), fgnd: ${this.fgndColor}`);
    }
    else { // MODE_ERASE or MODE_INVERSE
      this.context.fillStyle = this.bgndColor;
      this.debug(`draw line ERASE/INVERSE: (${x1},${y1}) -> (${x2},${y2}), bgnd: ${this.bgndColor}`);
    }
    // Define differences and error check
    let dx = Math.abs(x2 - x1);
    let dy = Math.abs(y2 - y1);
    let sx = (x1 < x2) ? 1 : -1;
    let sy = (y1 < y2) ? 1 : -1;
    let err = dx - dy;
    // Set first coordinates
    this.context.fillRect(x1, y1, 1, 1);
    // Main loop
    while (!((x1 == x2) && (y1 == y2))) {
      let e2 = err << 1;
      if (e2 > -dy) {
        err -= dy;
        x1 += sx;
      }
      if (e2 < dx) {
        err += dx;
        y1 += sy;
      }
      // Set coordinates
      this.context.fillRect(x1, y1, 1, 1);
    }
  }

  clearScreen() {
    this.context.fillStyle = this.bgndColor;
    this.context.fillRect(0, 0, 512, 512);
  }

  drawBlock(x1, y1, x2, y2) {
    this.X = x1;
    this.Y = y1 - 15;
    let x = (x1 < x2) ? x1 : x2;
    let y = 511 - ((y1 > y2) ? y1 : y2);
    let w = Math.abs(x1 - x2);
    let h = Math.abs(y1 - y2);
    if (this.graphicsMode === this.MODE_WRITE || this.graphicsMode === this.MODE_REWRITE) {
      this.context.fillStyle = this.fgndColor;
      this.debug(`draw block RE/WRITE: (${x},${y}) ${w}x${h}", fgnd: ${this.fgndColor}`);
    }
    else { // MODE_ERASE or MODE_INVERSE
      this.context.fillStyle = this.bgndColor;
      this.debug(`draw block ERASE/INVERSE: (${x},${y}) ${w}x${h}", bgnd: ${this.bgndColor}`);
    }
    this.context.fillRect(x, y, w, h);
  }

  renderBlockErase(word) {
    if (typeof this.blockStart === "undefined") {
      this.blockStart = [(word >> 9) & 0o777, word & 0o777];
    }
    else {
      let x1 = this.blockStart[0];
      let y1 = this.blockStart[1];
      let x2 = (word >> 9) & 0o777;
      let y2 = word & 0o777;
      delete this.blockStart;
      this.drawBlock(x1, y1, x2, y2);
    }
  }

  renderChar(c6) {
    if (c6 === 0o77) {
      this.charState = this.CHAR_STATE_ESCAPE;
      return;
    }
    if (this.charState === this.CHAR_STATE_NORMAL) {
      this.drawChar(c6, this.X, this.Y);
    }
    else {
      this.charState = this.CHAR_STATE_NORMAL;
      switch (c6) {
      case 0o10:   // backspace
        this.X = (this.X - this.fontWidth) & 0x1ff;
        this.debug(`<BS> (${this.X},${this.Y})`);
        break;
      case 0o11:   // tab
        this.X = (this.X + this.fontWidth) & 0x1ff;
        this.debug(`<HT> (${this.X},${this.Y})`);
        break;
      case 0o12:   // linefeed
        this.Y = (this.Y - (this.charSize === this.CHAR_SIZE_0 ? 16 : 32)) & 0x1ff;
        this.debug(`<LF> (${this.X},${this.Y})`);
        break;
      case 0o13:   // vertical tab
        this.Y = (this.Y + (this.charSize === this.CHAR_SIZE_0 ? 16 : 32)) & 0x1ff;
        this.debug(`<VT> (${this.X},${this.Y})`);
        break;
      case 0o14:   // form feed
        if (this.orientation === this.ORIENTATION_HORIZONTAL) {
          this.Y = 512 - (this.charSize === this.CHAR_SIZE_0 ? 16 : 32);
          if (this.direction === this.DIRECTION_REVERSE) {
            this.X = 512 - (this.charSize === this.CHAR_SIZE_0 ? 16 : 32);
          }
          else {
            this.X = this.margin[0];
          }
        }
        else {
          this.X = (this.charSize === this.CHAR_SIZE_0 ? 16 : 32) - 1;
          if (this.direction === this.DIRECTION_REVERSE) {
            this.Y = 512 - (this.charSize === this.CHAR_SIZE_0 ? 16 : 32);
          }
          else {
            this.Y = this.margin[1];
          }
        }
        this.debug(`<FF> (${this.X},${this.Y})`);
        break;
      case 0o15:   // carriage return
        if (this.orientation === this.ORIENTATION_HORIZONTAL) {
          this.X = this.margin[0];
          this.Y = (this.Y - (this.charSize === this.CHAR_SIZE_0 ? 16 : 32)) & 0x1ff;
        }
        else {
          this.X = (this.X + (this.charSize === this.CHAR_SIZE_0 ? 16 : 32)) & 0x1ff;
          this.Y = this.margin[1];
        }
        this.debug(`<CR> (${this.X},${this.Y})`);
        break;
      case 0o16:
        this.debug("superscript");
        // TODO: Superscript - accommodate orientation
        this.Y += 5;
        break;
      case 0o17:
        this.debug("subscript");
        // TODO: Subscript - accommodate orientation
        this.Y -= 5;
        break;
      case 0o20:
        this.debug("select charset M0");
        this.charset = this.CHARSET_M0;
        break;
      case 0o21:
        this.debug("select charset M1");
        this.charset = this.CHARSET_M1;
        break;
      case 0o22:
        this.debug("select charset M2");
        this.charset = this.CHARSET_M2;
        break;
      case 0o23:
        this.debug("select charset M3");
        this.charset = this.CHARSET_M3;
        break;
      case 0o24:
        this.debug("select charset M4");
        this.charset = this.CHARSET_M4;
        break;
      case 0o25:
        this.debug("select charset M5");
        this.charset = this.CHARSET_M5;
        break;
      case 0o26:
        this.debug("select charset M6");
        this.charset = this.CHARSET_M6;
        break;
      case 0o27:
        this.debug("select charset M7");
        this.charset = this.CHARSET_M7;
        break;
        break;
      case 0o30:
        this.debug("horizontal orientation");
        this.orientation = this.ORIENTATION_HORIZONTAL;
        break;
      case 0o31:
        this.debug("vertical orientation");
        this.orientation = this.ORIENTATION_VERTICAL;
        break;
      case 0o32:
        this.debug("forward writing");
        this.direction = this.DIRECTION_FORWARD;
        break;
      case 0o33:
        this.debug("reverse writing");
        this.direction = this.DIRECTION_REVERSE;
        break;
      case 0o34:
        this.debug("normal character size");
        this.charSize = this.CHAR_SIZE_0;
        break;
      case 0o35:
        this.debug("double character size");
        this.charSize = this.CHAR_SIZE_2;
        break;
      default:
        this.debug(`unrecognized escape code: ${this.toOctal(c6, 3)}`);
        break;
      }
    }
  }

  renderLine(word) {
    let x = (word >> 9) & 0o777;
    let y = word & 0o777;
    this.drawLine(this.X, this.Y, x, y);
    this.X = x;
    this.Y = y;
  }

  renderWord(word) {
    switch (this.renderMode) {
    case this.MODE_POINT:
      this.drawPoint((word >> 9) & 0o777, word & 0o777);
      break;
    case this.MODE_LINE:
      this.renderLine(word);
      break;
    case this.MODE_PLOT_CHARS:
      this.renderChar((word >> 12) & 0o77);
      this.renderChar((word >>  6) & 0o77);
      this.renderChar(word & 0o77);
      break;
    case this.MODE_BLOCK_ERASE:
      this.renderBlockErase(word);
      break;
    case this.MODE_WRITE_MEMORY:
      word &= 0xffff;
      this.ram[this.ramAddress] = word & 0xff;
      this.ram[this.ramAddress + 1] = word >> 8;
      if (this.ramAddress === this.C2_ORIGIN) {
        this.c2origin = word;
        this.debug("C2 origin", word);
      }
      else if (this.ramAddress === this.C3_ORIGIN) {
        this.c3origin = word;
        this.debug("C3 origin", word);
      }
      if (this.ramAddress >= this.c2origin && this.ramAddress < this.c2origin + (64 * 16)) {
        let i = (this.ramAddress - this.c2origin) / 2;
        let chIdx = i >> 3;
        let col = i & 7;
        this.alternateCharset[chIdx][col] = word;
        this.debug(`C2[${chIdx}][${col}]`, word);
      }
      else if (this.ramAddress >= this.c3origin && this.ramAddress < this.c3origin + (64 * 16)) {
        let i = (this.ramAddress - this.c3origin) / 2;
        let chIdx = (i >> 3) + 64;
        let col = i & 7;
        this.alternateCharset[chIdx][col] = word;
        this.debug(`C3[${chIdx - 64}][${col}]`, word);
      }
      else if (this.isDebug) {
        this.debug(`Store RAM[${this.toHex(this.ramAddress)}]: ${this.toHex(word & 0xff)} ${this.toHex(word >> 8)}`);
      }
      this.ramAddress += 2;
      break;
    case this.MODE_PROGRAM_ORG5:
    case this.MODE_PROGRAM_ORG6:
    case this.MODE_PROGRAM_ORG7:
      this.debug(`Render PC (${this.renderMode}) : ${this.toOctal(word, 7)}`);
      break;
    default:
      console.log(`Invalid render mode: ${this.renderMode}`);
      this.renderMode = this.MODE_PLOT_CHARS;
      break;
    }
  }

  processWord(word) {
    if (this.isDebug) console.log(`PLATO word: ${this.toOctal(word, 7)}`);

    if ((word & this.NOP_MASK) !== 0) { // NOP command
      if ((word & 1) !== 0) {
        this.wc = (this.wc + 1) & 0o177;
      }
    }
    else {
      this.wc = (this.wc + 1) & 0o177;
    }
    if ((word & 0o1000000) !== 0) {
      this.renderWord(word);
      return;
    }

    switch ((word >> 15) & 7) {
    case 0: // Nop
      switch (word & this.NOP_MASK_DATA) {
      case this.NOP_SET_STATION:
        this.station = word & 0o777;
        this.debug(`Set station ${this.toOctal(this.station, 3)}`);
        break;
      case this.NOP_META_START:
        this.collectedBytes = [];
        this.collectedBytes.push(this.cdcToAscii[word & 0x77]);
        break;
      case this.NOP_META_STREAM:
        this.collectedBytes.push(this.cdcToAscii[word & 0x77]);
        break;
      case this.NOP_META_STOP:
        this.collectedBytes.push(this.cdcToAscii[word & 0x77]);
        // TODO: process collected metadata
        break;
      case this.NOP_FONT_TYPE:
        this.setFontId(word & 0o77);
        this.debug(`Set font type ${this.toOctal(word & 0o77, 2)}`);
        break;
      case this.NOP_FONT_SIZE:
        this.setFontSize(word & 0o77);
        this.debug(`Set font size ${this.toOctal(word & 0o77, 2)}`);
        break;
      case this.NOP_FONT_FLAG:
        this.setFontFlags(word & 0o77);
        this.debug(`Set font flags ${this.toOctal(word & 0o77, 2)}`);
        break;
      case this.NOP_FONT_INFO:
        // TODO: send font width and height
        break;
      case this.NOP_OS_INFO:
        // TODO: send OS type, major ver, minor ver
        break;
      default:
        this.debug(`Unrecognized NOP code: word ${word} NOP code ${word & this.NOP_MASK_DATA}`);
        break;
      }
      break;
    case 1: // Load mode
      if ((word & 0o20000) !== 0) {
        this.wc = (word >> 6) & 0o177;
      }
      this.graphicsMode = (word >> 1) & 0o03;
      this.renderMode = (word >> 3) & 0o03;
      this.debug(`Load mode : graphics ${this.graphicsMode}, render ${this.renderMode}`);
      if ((word & 1) !== 0) this.clearScreen();
      break;
    case 2: // Load coordinate
      const offset = word & 0o777;
      const isY = (word & 0o1000) !== 0;
      if ((word & 0o4000) !== 0) { // Add or subtract from current coordinate
        if ((word & 0o2000) !== 0) {
          if (isY)
            this.Y -= offset;
          else
            this.X -= offset;
        }
        else {
          if (isY)
            this.Y += offset;
          else
            this.X += offset;
        }
      }
      else {
        if (isY)
          this.Y = offset;
        else
          this.X = offset;
      }
      if (isY)
        this.debug(`Set Y : ${this.Y}`);
      else
        this.debug(`Set X : ${this.X}`);
      if ((word & 0o10000) !== 0) {
      // TODO:  this.setRamMargin(isY ? this.Y : this.X);
      }
      break;
    case 3: // Echo
      word &= 0o177;
      this.debug(`Echo request ${word}`);
      switch (word) {
      case 0o160: // query terminal type
        this.debug("  Query terminal type");
        word = word + this.TERMINAL_TYPE;
        break;
      case 0x7b: // beep
        this.debug("  Beep");
        return;
      case 0x7d: // report RAM address
        this.debug("  Report RAM address");
        word = ramAddress;
        break;
      default:
        break;
      }
      this.debug(`Echo response ${word}`);
      this.send([0x80 | word]);
      break;
    case 4: // Load address
      this.ramAddress = word & 0o77777;
      this.debug(`Load address : ${this.toOctal(this.ramAddress, 6)}`);
      break;
    case 5: // SSF on PPT
      if (((word >> 10) & 0o37) === 1) {
        this.touchPanelEnabled = (word & 0o40) !== 0;
        this.debug(`SSF touch panel : ${this.touchPanelEnabled}`);
      }
      else {
        this.debug(`SSF : ${this.toOctal(word, 7)}`);
      }
      break;
    case 6:
    case 7:
    default:
      // do nothing
      break;
    }
  }

  touchPanelEventHandler(event) {
    function getOffset(el) {
      const rect = el.getBoundingClientRect();
      return {
        left: rect.left + window.scrollX,
        top: rect.top + window.scrollY
      };
    }
    if (this.touchPanelEnabled) {
      let offset = getOffset(event.target);
      let x = (event.clientX - offset.left) >> 5;
      if (x > 15) x = 15;
      let y = (511 - (event.clientY - offset.top)) >> 5;
      if (y > 15) y = 15;
      this.send([0x100 | (x << 4) | y]);
      this.debug(`touch panel event: (${x},${y})`);
    }
  }

  renderText(bytes) {
    let len = this.receivedBytes.length;
    if (typeof bytes === "string") {
      for (let i = 0; i < bytes.length; i++) {
        this.receivedBytes.push(bytes.charCodeAt(i));
      }
    }
    else {
      for (const b of bytes.values()) {
        this.receivedBytes.push(b);
      }
    }
    this.debug("received:", this.receivedBytes.slice(len));
    while (this.receivedBytes.length > 2) {
      let word = 0;
      let b = this.receivedBytes.shift();
      if ((b & 0x80) !== 0) {
        console.log(`Received data out of sync, byte 0 : ${this.toHex(b)}`);
      }
      word = (b & 0x7f) << 12;
      b = this.receivedBytes.shift();
      if ((b & 0xc0) !== 0x80) {
        console.log(`Received data out of sync, byte 1 : ${this.toHex(b)}`);
      }
      word |= (b & 0x3f) << 6;
      b = this.receivedBytes.shift();
      if ((b & 0xc0) !== 0xc0) {
        console.log(`Received data out of sync, byte 2 : ${this.toHex(b)}`);
      }
      word |= b & 0x3f;
      this.processWord(word);
    }
  }
}
//
// The following lines enable this file to be used as a Node.js module.
//
if (typeof module === "undefined") module = {};
module.exports = PTermClassic;
