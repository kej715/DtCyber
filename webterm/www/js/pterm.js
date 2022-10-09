/*--------------------------------------------------------------------------
**
**  Copyright (c) 2019, Kevin Jordan
**
**  pterm.js
**    This class implements a PLATO/CYBIS terminal emulator.
**
**--------------------------------------------------------------------------
*/

class PTerm {

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

    this.COORD_HIGH_Y = 0x20;
    this.COORD_LOW_Y  = 0x60;
    this.COORD_HIGH_X = 0x20;
    this.COORD_LOW_X  = 0x40;

    this.DIRECTION_FORWARD = 0;
    this.DIRECTION_REVERSE = 1;

    this.KEY_ECHO_RESPONSE      = 0x000;
    this.KEY_TOUCH_PANEL_DATA   = 0x100;
    this.KEY_EXTERNAL_DATA      = 0x200;
    this.KEY_UNSOLICITED_STATUS = 0x300;

    this.MODE_WRITE   = 0;
    this.MODE_ERASE   = 1;
    this.MODE_REWRITE = 2;
    this.MODE_INVERSE = 3;

    this.MODE_NONE           = 0;
    this.MODE_ALPHA          = 1;
    this.MODE_POINT          = 2;
    this.MODE_LINE           = 3;
    this.MODE_BLOCK          = 4;
    this.MODE_LOAD_MEMORY    = 5;
    this.MODE_LOAD_CHARSET   = 6;

    this.ORIENTATION_HORIZONTAL = 0;
    this.ORIENTATION_VERTICAL   = 1;

    this.RECEIVE_STATE_NORMAL = 0;
    this.RECEIVE_STATE_IAC    = 1;
    this.RECEIVE_STATE_CR     = 2;
    this.isDtCyberPterm       = true;
    this.receiveState         = this.RECEIVE_STATE_NORMAL;

    this.bgndColor     = "#000000"; // black
    this.canvas        = null;
    this.charArray     = [];
    this.charHeight    = 16;
    this.charWidth     = 8;
    this.context       = null;
    this.fgndColor     = "#ffa500"; // orange
    this.inverse       = false;
    this.isDebug       = false;
    this.lastX         = 0;  // last computed X coordinate
    this.lastY         = 0;  // last computed Y coordinate
    this.termSubtype   = 16; // Pterm
    this.X             = 0;  // current X coordinate
    this.Y             = 0;  // current Y coordinate

    this.ASCIIcharset  = [
      0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f, // <NUL> - <SI>
      0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f, // <DLE> - <US>
      0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f, // <SP> - /
      0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f, // 0 - ?
      0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f, // @ - O
      0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f, // P - _
      0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f, // ` - o
      0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f  // p - <DEL>
    ];

    this.PLATOcharset  = [
      0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07, // <NUL> - <BEL>
      0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f, // <BS>  - <SI>
      0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17, // <DLE> - <ETB>
      0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f, // <CAN> - <US>
      0x20,0x2f,0x3f,0x6e,0x32,0x3d,0x57,0x44, //    20 - 27
      0x58,0x41,0x2a,0x2b,0x2d,0x33,0x34,0x2f, //    28 - 2f
      0x61,0x62,0x64,0x6c,0x6d,0x70,0x72,0x73, //    30 - 37
      0x77,0x3c,0x3e,0x74,0x30,0x2e,0x31,0x36, //    38 - 3f
      0x43,0x75,0x4f,0x78,0x71,0x2a,0x65,0x63, //    40 - 47
      0x76,0x2c,0x49,0x00,0x00,0x00,0x00,0x00, //    48 - 4f
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, //    50 - 57
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, //    58 - 5f
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, //    60 - 67
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, //    68 - 6f
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, //    70 - 77
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00  //    78 - 7f
    ];

    this.platoKeyMap = [
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // <NUL> - <SI>
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // <DLE> - <US>
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // <SP> - /
      0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 0 - ?
      0x00,0x00,0x0e,0x16,0x1d,0x18,0x00,0x00,0x09,0x00,0x00,0x00,0x0f,0x00,0x1e,0x00, // @ - O
      0x17,0x00,0x19,0x11,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00, // P - _
      0x00,0x07,0x02,0x03,0x12,0x1a,0x7f,0x00,0x0b,0x00,0x00,0x00,0x0c,0x7b,0x0d,0x00, // ` - o
      0x13,0x00,0x08,0x01,0x14,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00  // p - <DEL>
    ];

    this.charsets  = [this.ASCIIcharset, this.PLATOcharset];
    this.fontNames = ["PlatoAscii", "PlatoMicro"];

    this.parityMap = new Uint8Array(128);
    const masks = [0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01];
    for (let b = 0; b < 128; b++) {
      let n = 0;
      for (let i = 0; i < masks.length; i++) {
        if (b & masks[i]) n++;
      }
      this.parityMap[b] = (n & 1) ? b|0x80 : b;
    }
  }

  toHex(value) {
    if (Array.isArray(value)) {
      let result = [];
      for (let i = 0; i < value.length; i++) {
        let byte = value[i];
        result.push((byte < 16) ? "0" + byte.toString(16) : byte.toString(16));
      }
      return result;
    }
    else {
      return (value < 16) ? "0" + value.toString(16) : value.toString(16);
    }
  }

  debug(message, bytes) {
    if (this.isDebug) {
      if (bytes) {
        console.log(message + ": [" + this.toHex(bytes).join(" ") + "]");
      }
      else {
        console.log(message);
      }
    }
  }

  setDebug(db) {
    this.isDebug = db;
  }

  setTermSubtype(subtype) {
    this.termSubtype = subtype;
  }

  setUplineDataSender(callback) {
    this.uplineDataSender = callback;
  }

  setFont() {
    this.context.font = "normal 16px " + this.fontNames[this.charset];
  }

  processKeyboardEvent(keyStr, shiftKey, ctrlKey, altKey) {
    let bytes = [];
    //
    // Handle non-special keys
    //
    if (keyStr.length < 2) { // non-special key
      let byte = keyStr.charCodeAt(0);
      if (ctrlKey) {
        byte = (byte < 0x80) ? this.platoKeyMap[byte] : 0;
      }
      if (byte) {
        bytes.push(byte);
      }
    }
    //
    // Handle special keys
    //
    else {

      switch (keyStr) {
      case "Backspace":
        bytes.push(shiftKey ? 0x19 : 0x08);
        break;
      case "Delete":
        bytes.push(shiftKey ? 0x19 : 0x08);
        break;
      case "Enter":
        bytes.push(shiftKey ? 0x1e : 0x0d);
        break;
      case "Escape":
        break;
      case "Tab":
        break;
      case "ArrowUp":
        break;
      case "ArrowDown":
        break;
      case "ArrowRight":
        break;
      case "ArrowLeft":
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
      this.debug("send", bytes);
      let buffer = new Uint8Array(bytes.length);
      for (let i = 0; i < bytes.length; i++) {
        buffer[i] = this.parityMap[bytes[i] & 0x7f];
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

  initPlatoMode() {
    this.bgndColor         = "#000000"; // black
    this.charset           = this.CHARSET_M0;
    this.charSize          = this.CHAR_SIZE_0;
    this.direction         = this.DIRECTION_FORWARD;
    this.renderMode        = this.MODE_NONE;
    this.fgndColor         = "#ffa500"; // orange
    this.graphicsMode      = this.MODE_WRITE;
    this.lastX             = 0;
    this.lastY             = 0;
    this.margin            = [0, 0];
    this.orientation       = this.ORIENTATION_HORIZONTAL;
    this.touchPanelEnabled = false;
    this.X                 = 0;
    this.Y                 = 496;

    this.alternateCharset = [];
    for (let i = 0; i < 128; i++) {
      let char = [];
      for (let j = 0; j < 8; j++) {
        char.push(0);
      }
      this.alternateCharset.push(char);
    }
  }

  reset() {
    this.initPlatoMode();
    this.charHeight = 16;
    this.charWidth  = 8;
    this.renderer   = this.ttyMode;
    this.context.fillStyle = this.bgndColor;
    this.context.fillRect(0, 0, 512, 512);
    this.setFont();
  }

  drawStdChar(chCode, x, y) {
    this.context.textBaseline = "bottom";
    if (this.graphicsMode === this.MODE_WRITE) {
      this.context.fillStyle = this.fgndColor;
      this.context.fillText(String.fromCharCode(this.charsets[this.charset][chCode]), x, 511 - y);
      if (this.isDebug) {
        let s = (chCode >= 0x20 && chCode < 0x7f) ? String.fromCharCode(chCode) : ".";
        this.debug("drawStdChar WRITE " + s + " <" + this.toHex(chCode) + "> at (" + x + "," + y + "), " +
                   "charset: M" + this.charset + ", fgnd: " + this.fgndColor);
      }
    }
    else if (this.graphicsMode === this.MODE_ERASE) {
      this.context.fillStyle = this.bgndColor;
      this.context.fillText(String.fromCharCode(this.charsets[this.charset][chCode]), x, 511 - y);
      if (this.isDebug) {
        let s = (chCode >= 0x20 && chCode < 0x7f) ? String.fromCharCode(chCode) : ".";
        this.debug("drawStdChar ERASE " + s + " <" + this.toHex(chCode) + "> at (" + x + "," + y + "), " +
                   "charset: M" + this.charset + ", bgnd: " + this.bgndColor);
      }
    }
    else if (this.graphicsMode === this.MODE_REWRITE) {
      this.context.fillStyle = this.bgndColor;
      this.context.fillRect(x, 511 - y - 15, this.charWidth, this.charHeight);
      this.context.fillStyle = this.fgndColor;
      this.context.fillText(String.fromCharCode(this.charsets[this.charset][chCode]), x, 511 - y);
      if (this.isDebug) {
        let s = (chCode >= 0x20 && chCode < 0x7f) ? String.fromCharCode(chCode) : ".";
        this.debug("drawStdChar REWRITE " + s + " <" + this.toHex(chCode) + "> at (" + x + "," + y + "), " +
                   "charset: M" + this.charset + ", fgnd: " + this.fgndColor + ", bgnd: " + this.bgndColor);
      }
    }
    else { // MODE_INVERSE
      this.context.fillStyle = this.fgndColor;
      this.context.fillRect(x, 511 - y - 15, this.charWidth, this.charHeight);
      this.context.fillStyle = this.bgndColor;
      this.context.fillText(String.fromCharCode(this.charsets[this.charset][chCode]), x, 511 - y);
      if (this.isDebug) {
        let s = (chCode >= 0x20 && chCode < 0x7f) ? String.fromCharCode(chCode) : ".";
        this.debug("drawStdChar INVERSE " + s + " <" + this.toHex(chCode) + "> at (" + x + "," + y + "), " +
                   "charset: M" + this.charset + ", fgnd: " + this.fgndColor + ", bgnd: " + this.bgndColor);
      }
    }
  }

  drawAltChar(chCode, x, y) {
    chCode -= 0x20;
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
      this.debug("drawAltChar WRITE <" + this.toHex(chCode) + "> at (" + x + "," + y + "), " +
                 "charset: M" + this.charset + ", size: " + this.charSize +
                 ", fgnd: " + this.fgndColor + ", bgnd: " + this.bgndColor);
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
      this.debug("drawAltChar ERASE <" + this.toHex(chCode) + "> at (" + x + "," + y + "), " +
                 "charset: M" + this.charset + ", size: " + this.charSize +
                 ", fgnd: " + this.fgndColor + ", bgnd: " + this.bgndColor);
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
      this.debug("drawAltChar REWRITE <" + this.toHex(chCode) + "> at (" + x + "," + y + "), " +
                 "charset: M" + this.charset + ", size: " + this.charSize +
                 ", fgnd: " + this.fgndColor + ", bgnd: " + this.bgndColor);
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
      this.debug("drawAltChar INVERSE <" + this.toHex(chCode) + "> at (" + x + "," + y + "), " +
                 "charset: M" + this.charset + ", size: " + this.charSize +
                 ", fgnd: " + this.fgndColor + ", bgnd: " + this.bgndColor);
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
    this.X = (x + this.charWidth) & 0x1ff; // TODO: accommodate direction and orientation
    this.Y = y;
  }

  drawPoint(x, y) {
    if (this.graphicsMode === this.MODE_WRITE || this.graphicsMode === this.MODE_REWRITE) {
      this.context.fillStyle = this.fgndColor;
      this.debug("draw point RE/WRITE: (" + x + "," + y + "), fgnd: " + this.fgndColor);
    }
    else { // MODE_ERASE or MODE_INVERSE
      this.context.fillStyle = this.bgndColor;
      this.debug("draw point ERASE/INVERSE: (" + x + "," + y + "), bgnd: " + this.bgndColor);
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
      this.debug("draw line RE/WRITE: (" + x1 + "," + y1 + ") -> (" + x2 + "," + y2 + "), fgnd: " + this.fgndColor);
    }
    else { // MODE_ERASE or MODE_INVERSE
      this.context.fillStyle = this.bgndColor;
      this.debug("draw line ERASE/INVERSE: (" + x1 + "," + y1 + ") -> (" + x2 + "," + y2 + "), bgnd: " + this.bgndColor);
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

  drawBlock(x1, y1, x2, y2) {
    this.X = x1;
    this.Y = y1 - 15;
    let x = (x1 < x2) ? x1 : x2;
    let y = 511 - ((y1 > y2) ? y1 : y2);
    let w = Math.abs(x1 - x2);
    let h = Math.abs(y1 - y2);
    if (this.graphicsMode === this.MODE_WRITE || this.graphicsMode === this.MODE_REWRITE) {
      this.context.fillStyle = this.fgndColor;
      this.debug("draw block RE/WRITE: (" + x + "," + y + ") " + w + "x" + h + ", fgnd: " + this.fgndColor);
    }
    else { // MODE_ERASE or MODE_INVERSE
      this.context.fillStyle = this.bgndColor;
      this.debug("draw block ERASE/INVERSE: (" + x + "," + y + ") " + w + "x" + h + ", bgnd: " + this.bgndColor);
    }
    this.context.fillRect(x, y, w, h);
  }

  ttyMode(byte) {
    if (byte < 0x20) {
      switch (byte) {
      case 0x08: // <BS>
        this.X = (this.X - 8) & 0x1ff;
        break;
      case 0x0a: // <LF>
        if (this.Y > 0) {
          this.Y -= 16;
        }
        else {
          this.scrollUp();
        }
        break;
      case 0x0d: // <CR>
        this.X = 0;
        break;
      case 0x1b: // <ESC>
        this.renderer = this.ttyModeEsc;
        break;
      default: // ignore all other control characters
        break;
      }
    }
    else if (byte < 0x7f && this.X < 512) {
      this.drawChar(byte, this.X, this.Y);
    }
  }

  scrollUp() {
    let charBlock = this.context.getImageData(0, 16, 512, 496);
    this.context.putImageData(charBlock, 0, 0);
    this.context.fillStyle = this.bgndColor;
    this.context.fillRect(0, 496, 512, 16);
  }

  ttyModeEsc(byte) {
    if (byte === 0x02) { // <STX>
      this.debug("enter Plato mode", [0x1b, 0x02]);
      this.initPlatoMode();
      this.renderer = this.platoMode;
    }
    else if (byte !== 0x1b) {
      this.renderer = this.ttyMode;
    }
  }

  platoMode(byte) {
    if (byte < 0x20) {
      switch (byte) {
      case 0x08: // <BS>
        this.X = (this.X - 8) & 0x1ff;
        this.debug("plato mode <BS> (" + this.X + "," + this.Y + ")");
        break;
      case 0x09: // <HT>
        this.X = (this.X + 8) & 0x1ff;
        this.debug("plato mode <HT> (" + this.X + "," + this.Y + ")");
        break;
      case 0x0a: // <LF>
        this.Y = (this.Y - 16) & 0x1ff;
        this.debug("plato mode <LF> (" + this.X + "," + this.Y + ")");
        break;
      case 0x0b: // <VT>
        this.Y = (this.Y + 16) & 0x1ff;
        this.debug("plato mode <VT> (" + this.X + "," + this.Y + ")");
        break;
      case 0x0c: // <FF>
        this.X = 0;
        this.Y = 496;
        this.debug("plato mode <FF> (" + this.X + "," + this.Y + ")");
        break;
      case 0x0d: // <CR>
        if (this.orientation === this.ORIENTATION_HORIZONTAL) {
          this.X = this.margin[0];
          this.Y = (this.Y - (this.charSize === this.CHAR_SIZE_0 ? 16 : 32)) & 0x1ff;
        }
        else {
          this.X = (this.X + (this.charSize === this.CHAR_SIZE_0 ? 16 : 32)) & 0x1ff;
          this.Y = this.margin[1];
        }
        this.debug("plato mode <CR> (" + this.X + "," + this.Y + ")");
        break;
      case 0x19: // <EM>
        this.beginCollection();
        this.blockCorners = [];
        this.renderMode = this.MODE_BLOCK;
        this.debug("enter block mode");
        break;
      case 0x1b: // <ESC>
        this.renderer = this.platoModeEsc;
        break;
      case 0x1c: // <FS>
        this.beginCollection();
        this.renderMode = this.MODE_POINT;
        this.debug("enter point mode");
        break;
      case 0x1d: // <GS>
        this.beginCollection();
        delete this.lastLineCoordinate;
        this.renderMode = this.MODE_LINE;
        this.debug("enter line mode");
        break;
      case 0x1f: // <US>
        this.renderMode = this.MODE_ALPHA;
        this.debug("enter alpha mode");
        break;
      default: // ignore all other control characters
        this.debug("plato mode discard <" + this.toHex(byte) + ">");
        break;
      }
    }
    else {
      switch (this.renderMode) {
      case this.MODE_ALPHA:
        if (byte < 0x7f) {
          this.drawChar(byte, this.X, this.Y);
        }
        else {
          this.debug("plato alpha mode discard <" + this.toHex(byte) + ">");
        }
        break;
      case this.MODE_POINT:
        this.platoModePointPlot(byte);
        break;
      case this.MODE_LINE:
        this.platoModeDrawLine(byte);
        break;
      case this.MODE_BLOCK:
        this.platoModeBlockDraw(byte);
        break;
      case this.MODE_LOAD_MEMORY:
        this.platoModeLoadMemory(byte);
        break;
      case this.MODE_LOAD_CHARSET:
        this.platoModeLoadCharset(byte);
        break;
      default:
        this.debug("discard <" + this.toHex(byte) + ">, draw mode: " + this.renderMode);
        break;
      }
    }
  }

  platoModeEsc(byte) {
    this.renderer = this.platoMode;
    switch (byte) {
    case 0x01: // <SOH>
      this.debug("enter Tektronix mode");
      this.renderer = this.tektronixMode; // TODO: implement Tektronix Mode
      this.initPlatoMode();
      this.setFont();
      this.renderer = this.ttyMode;
      break;
    case 0x02: // <STX>
      this.debug("enter Plato mode");
      this.initPlatoMode();
      this.setFont();
      break;
    case 0x03: // <ETX>
      this.debug("enter TTY mode");
      this.initPlatoMode();
      this.setFont();
      this.renderer = this.ttyMode;
      break;
    case 0x0c: // <FF>
      this.debug("clear screen");
      this.clearScreen();
      break;
    case 0x11: // <DC1>
      this.debug("mode inverse");
      this.graphicsMode = this.MODE_INVERSE;
      break;
    case 0x12: // <DC2>
      this.debug("mode write");
      this.graphicsMode = this.MODE_WRITE;
      break;
    case 0x13: // <DC3>
      this.debug("mode erase");
      this.graphicsMode = this.MODE_ERASE;
      break;
    case 0x14: // <DC4>
      this.debug("mode rewrite");
      this.graphicsMode = this.MODE_REWRITE;
      break;
    case 0x1b: // <ESC>
      this.renderer = this.platoModeEsc;
      break;
    case 0x32: // 2
      this.beginCollection();
      this.renderer = this.platoModeLoadCoordinate;
      break;
    case 0x40: // @
      this.debug("superscript");
      // TODO: Superscript - accommodate orientation
      this.Y += 5;
      break;
    case 0x41: // A
      this.debug("subscript");
      // TODO: Subscript - accommodate orientation
      this.Y -= 5;
      break;
    case 0x42: // B
      this.debug("charset M0");
      this.charset = this.CHARSET_M0;
      this.setFont();
      break;
    case 0x43: // C
      this.debug("charset M1");
      this.charset = this.CHARSET_M1;
      this.setFont();
      break;
    case 0x44: // D
      this.debug("charset M2");
      this.charset = this.CHARSET_M2;
      break;
    case 0x45: // E
      this.debug("charset M3");
      this.charset = this.CHARSET_M3;
      break;
    case 0x46: // F
      this.debug("charset M4");
      this.charset = this.CHARSET_M4;
      break;
    case 0x47: // G
      this.debug("charset M5");
      this.charset = this.CHARSET_M5;
      break;
    case 0x48: // H
      this.debug("charset M6");
      this.charset = this.CHARSET_M6;
      break;
    case 0x49: // I
      this.debug("charset M7");
      this.charset = this.CHARSET_M7;
      break;
    case 0x4a: // J
      this.debug("orientation horizontal");
      this.orientation = this.ORIENTATION_HORIZONTAL;
      break;
    case 0x4b: // K
      this.debug("orientation vertical");
      this.orientation = this.ORIENTATION_VERTICAL;
      break;
    case 0x4c: // L
      this.debug("direction foreward");
      this.direction = this.DIRECTION_FORWARD;
      break;
    case 0x4d: // M
      this.debug("direction backward");
      this.direction = this.DIRECTION_REVERSE;
      break;
    case 0x4e: // N
      this.debug("char size 0");
      this.charSize = this.CHAR_SIZE_0;
      break;
    case 0x4f: // O
      this.debug("char size 2");
      this.charSize = this.CHAR_SIZE_2;
      break;
    case 0x50: // P
      this.debug("enter mode load character set");
      this.beginCollection();
      this.renderMode = this.MODE_LOAD_CHARSET;
      break;
    case 0x51: // Q
      this.beginCollection();
      this.renderer = this.platoModeSSF;
      break;
    case 0x52: // R
      this.beginCollection();
      this.renderer = this.platoModeEXT;
      break;
    case 0x53: // S
      this.debug("enter mode load memory");
      this.beginCollection();
      this.renderMode = this.MODE_LOAD_MEMORY;
      break;
    case 0x54: // T
      this.debug("mode 5");
      break;
    case 0x55: // U
      this.debug("mode 6");
      break;
    case 0x56: // V
      this.debug("mode 7");
      break;
    case 0x57: // W
      this.beginCollection();
      this.renderer = this.platoModeLoadMemoryAddress;
      break;
    case 0x59: // Y
      this.beginCollection();
      this.renderer = this.platoModeEcho;
      break;
    case 0x5a: // Z
      this.debug("set margin: ("+this.X+","+this.Y+")");
      this.margin = [this.X, this.Y];
      break;
    case 0x61: // a
      this.beginCollection();
      this.renderer = this.platoModeFgndColor;
      break;
    case 0x62: // b
      this.beginCollection();
      this.renderer = this.platoModeBgndColor;
      break;
    case 0x63: // c
      this.beginCollection();
      this.renderer = this.platoModePaint;
      break;
    case 0x67: // c
      this.beginCollection();
      this.renderer = this.platoModeFgndGrayscale;
      break;
    default:
      this.debug("UNKNOWN Plato mode esc: " + this.toHex(byte));
      break;
    }
  }

  sendKey(keyData) {
    this.send([0x1b, 0x40 | (keyData & 0x3f), 0x60 | ((keyData >> 6) & 0x0f)]);
  }

  sendEchoResponse(response) {
    this.sendKey(this.KEY_ECHO_RESPONSE | 0x80 | response);
  }

  platoModeEcho(byte) {
    if (this.collectWord(byte)) {
      let request = ((this.collectedBytes[2] & 0x3f) << 12) |
                    ((this.collectedBytes[1] & 0x3f) <<  6) |
                     (this.collectedBytes[0] & 0x3f);
      switch (request) {
//    case 0x52: // Turn on flow control (probably no need to implement)
//      this.sendEchoResponse(0x53);
//      break;
      case 0x70: // Terminal type
        this.debug("echo request: terminal type");
        this.sendEchoResponse(12);
        break;
      case 0x71: // Terminal subtype
        this.debug("echo request: terminal subtype");
        //  0: Control Data IST II
        //  1: Control Data IST III
        //  2: Control Data 721-30 (Viking)
        //  3: Control Data IST I
        //  4: IBM PC
        //  5: Zenith Z100 (color)
        //  6: Apple II
        //  7: Scrolling terminal (dumb TTY)
        //  8: Dataspeed 40
        //  9: VT 100 or ANSI X.364 compatible terminal
        // 10: Unused
        // 11: CDC 750
        // 12: Tektronix 401x
        // 13: TI 99/4a
        // 14: Atari
        // 15: unknown
        // 16: Pterm
        this.sendEchoResponse(this.termSubtype);
        break;
      case 0x72: // Resident load file
        this.debug("echo request: resident load file");
        this.sendEchoResponse(0);
        break;
      case 0x73: // Terminal configuration
        this.debug("echo request: terminal configuration");
        this.sendEchoResponse(0x60); // 32K mem + touch panel
        break;
      case 0x7a: // Send backout code
        this.debug("echo request: send backout code");
        // TODO: send backout code and disconnect
        break;
      case 0x7b: // Sound alarm
        this.debug("echo request: sound alarm");
        // TODO: sound alarm and don't send a response
        break;
//    case 0x7c: // Return terminal ID code
//      break;
//    case 0x7d: // Read memory
//      break;
      default: // unrecognized echo request
        this.debug("echo request: unsupported " + this.toHex(request));
        this.sendEchoResponse(request);
        break;
      }
      this.renderer = this.platoMode;
    }
  }

  clearScreen() {
    this.context.fillStyle = this.bgndColor;
    this.context.fillRect(0, 0, 512, 512);
  }

  beginCollection() {
    this.collectedBytes = [];
  }

  collectColor(byte) {
    return this.collectedBytes.push(byte) >= 4;
  }

  collectCoordinate(byte) {
    this.collectedBytes.push(byte);
    return (byte & 0x60) === this.COORD_LOW_X;
  }

  collectWord(byte) {
    return this.collectedBytes.push(byte) >= 3;
  }

  computeCoordinate(collectedBytes) {
    let x = this.lastX;
    let y = this.lastY;
    let canBeHighY = true;
    for (let i = 0; i < collectedBytes.length; i++) {
      let byte = collectedBytes[i];
      switch (byte & 0x60) {
      case this.COORD_HIGH_Y:
      case this.COORD_HIGH_X:
        if (canBeHighY) {
          y = (y & 0x1f) | ((byte & 0x1f) << 5);
        }
        else {
          x = (x & 0x1f) | ((byte & 0x1f) << 5);
        }
        break;
      case this.COORD_LOW_Y:
        y = (y & 0x1e0) | (byte & 0x1f);
        canBeHighY = false;
        break;
      case this.COORD_LOW_X:
        x = (x & 0x1e0) | (byte & 0x1f);
        canBeHighY = false;
        break;
      default:
        cnosole.log("Invalid coordinates: " + JSON.stringify(collectedBytes));
        break;
      }
    }
    this.lastX = x;
    this.lastY = y;
    return [x, y];
  }

  computeColor(collectedBytes) {
    let rgb = ((collectedBytes[3] & 0x3f) << 18) |
              ((collectedBytes[2] & 0x3f) << 12) |
              ((collectedBytes[1] & 0x3f) <<  6) |
               (collectedBytes[0] & 0x3f);
    return "#" + this.toHex(rgb >> 16) +
                 this.toHex((rgb >> 8) & 0xff) +
                 this.toHex(rgb & 0xff);
  }

  platoModeFgndColor(byte) {
    if (this.collectColor(byte)) {
      this.fgndColor = this.computeColor(this.collectedBytes);
      this.renderer = this.platoMode;
      this.debug("set foreground color: " + this.fgndColor);
    }
  }

  platoModeBgndColor(byte) {
    if (this.collectColor(byte)) {
      this.bgndColor = this.computeColor(this.collectedBytes);
      this.renderer = this.platoMode;
      this.debug("set background color: " + this.bgndColor);
    }
  }

  platoModeFgndGrayscale(byte) {
    let intensity = this.toHex((byte & 0x3f) << 2);
    this.fgndColor = "#" + intensity + intensity + intensity;
    this.renderer = this.platoMode;
    this.debug("set foreground greyscale: " + this.fgndColor);
  }

  platoModeBlockDraw(byte) {
    if (this.collectCoordinate(byte)) {
      let coord = this.computeCoordinate(this.collectedBytes);
      this.beginCollection();
      this.blockCorners.push(coord);
      if (this.blockCorners.length > 1) {
        this.drawBlock(this.blockCorners[0][0], this.blockCorners[0][1],
                       this.blockCorners[1][0], this.blockCorners[1][1]);
        this.blockCorners = [];
      }
    }
  }

  paintWithColor(imageData, x, y, fgnd, bgnd) {
    let pixels = imageData.data;
    let width = imageData.width;
    let height = imageData.height;
    function isEdge(offset) {
      return pixels[offset] === bgnd[0] && pixels[offset + 1] === bgnd[1] && pixels[offset + 2] === bgnd[2] &&
        pixels[offset + 3] === 255;
    }
    function isFilled(offset) {
      return pixels[offset] === fgnd[0] && pixels[offset + 1] === fgnd[1] && pixels[offset + 2] === fgnd[2];
    }
    function fillPixel(x, y) {
      let neighbors = [];
      let offset = (y * width + x) * 4;
      if (!isEdge(offset) && !isFilled(offset)) {
        pixels.set(fgnd, offset);
        pixels[offset + 3] = 254; // to distinguish between filled and original pixels
        if (y > 0) { neighbors.push([x, y - 1]); }
        if (y + 1 < height) { neighbors.push([x, y + 1]); }
        if (x > 0) { neighbors.push([x - 1, y]); }
        if (x + 1 < width) { neighbors.push([x + 1, y]); }
      }
      return neighbors;
    }
    let toBeVisited = [[x, y]];
    while (toBeVisited.length > 0) {
      let coord = toBeVisited.shift();
      let neighbors = fillPixel(coord[0], coord[1]);
      while (neighbors.length > 0) { toBeVisited.push(neighbors.shift()); }
    }
    this.context.putImageData(imageData, 0, 0);
  }

  paintWithChar(imageData, x, y, charset, charIndex, fgnd, bgnd) {
    let pixels = imageData.data;
    let width = imageData.width;
    let yDelta = width * 4;
    let height = imageData.height;
    let charCoords = [];
    function addCharCoord(x, y) {
      let i = 0;
      while (i < charCoords.length) {
        let coord = charCoords[i];
        if (coord[0] === x && coord[1] === y) break;
        i++;
      }
      if (i >= charCoords.length) {
        charCoords.push([x, y]);
        return true;
      }
      else {
        return false;
      }
    }
    function isEdge(offset) {
      for (let i = 0; i < 16; i++) {
        if (pixels[offset] === bgnd[0] && pixels[offset + 1] === bgnd[1] && pixels[offset + 2] === bgnd[2]) {
          return true;
        }
        else {
          offset -= yDelta;
        }
      }
      for (let i = 0; i < 8; i++) {
        if (pixels[offset] === bgnd[0] && pixels[offset + 1] === bgnd[1] && pixels[offset + 2] === bgnd[2]) {
          return true;
        }
        else {
          offset += 4;
        }
      }
    }
    function testCharBlock(x, y) {
      let neighbors = [];
      let offset = (y * width + x) * 4;
      if ((y - 15) >= 0 && (x + 7) <= 511 && !isEdge(offset)) {
        if (addCharCoord(x, y)) {
          if (y > 0) { neighbors.push([x, y - 16]); }
          if (y + 16 < height) { neighbors.push([x, y + 16]); }
          if (x > 0) { neighbors.push([x - 8, y]); }
          if (x + 8 < width) { neighbors.push([x + 8, y]); }
        }
      }
      return neighbors;
    }
    let toBeVisited = [[x, y]];
    while (toBeVisited.length > 0) {
      let coord = toBeVisited.shift();
      let neighbors = testCharBlock(coord[0], coord[1]);
      while (neighbors.length > 0) { toBeVisited.push(neighbors.shift()); }
    }
    let savedX = this.X;
    let savedY = this.Y;
    let savedCharset = this.charset;
    let savedMode = this.graphicsMode;
    this.charset = charset;
    this.graphicsMode = this.MODE_WRITE;
    while (charCoords.length > 0) {
      let coord = charCoords.shift();
      this.drawChar(charIndex, coord[0], 511 - coord[1]);
    }
    this.X = savedX;
    this.Y = savedY;
    this.charset = savedCharset;
    this.graphicsMode = savedMode;
  }

  platoModePaint(byte) {
    if (this.collectedBytes.push(byte) > 1) {
      let command = ((this.collectedBytes[1] & 0x07) << 6) |
                     (this.collectedBytes[0] & 0x3f);
      let charset = command >> 7;
      let charIndex = command & 0x7f;
      let fgnd = [parseInt(this.fgndColor.substring(1, 3), 16),
                  parseInt(this.fgndColor.substring(3, 5), 16),
                  parseInt(this.fgndColor.substring(5, 7), 16)];
      let bgnd = [parseInt(this.bgndColor.substring(1, 3), 16),
                  parseInt(this.bgndColor.substring(3, 5), 16),
                  parseInt(this.bgndColor.substring(5, 7), 16)];
      let imageData = this.context.getImageData(0, 0, 512, 512);
      if (charIndex === 0) {
        this.paintWithColor(imageData, this.X, 511 - this.Y, fgnd, bgnd);
        this.debug("paint: (" + this.X + "," + this.Y + ") fgnd: " + this.fgndColor +
          ", bgnd: " + this.bgndColor);
      }
      else {
        this.paintWithChar(imageData, this.X, 511 - this.Y, charset, charIndex, fgnd, bgnd);
        this.debug("paint: (" + this.X + "," + this.Y + ") M" + charset + " <" + this.toHex(charIndex) + ">");
      }
      this.renderer = this.platoMode;
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
      let data = 0x100 | (x << 4) | y;
      this.send([0x1b, 0x40 | (data & 0x3f), 0x60 | (data >> 6)]);
      this.debug("touch panel event: (" + x + "," + y + ")");
    }
  }

  platoModeSSF(byte) {
    if (this.collectWord(byte)) {
      let command = ((this.collectedBytes[2] & 0x3f) << 12) |
                    ((this.collectedBytes[1] & 0x3f) <<  6) |
                     (this.collectedBytes[0] & 0x3f);
      command &= 0x7fff;
      this.debug("SSF: " + command.toString(16));
      if ((command >> 10) === 1) { // if serial channel
        if (command & 0x20) {
          this.touchPanelEnabled = true;
          this.debug("SSF: enable touch panel");
        }
        else {
          this.touchPanelEnabled = false;
          this.debug("SSF: disable touch panel");
        }
      }
      this.renderer = this.platoMode;
    }
  }

  platoModeEXT(byte) {
    if (this.collectWord(byte)) {
      let command = ((this.collectedBytes[2] & 0x3f) << 12) |
                    ((this.collectedBytes[1] & 0x3f) <<  6) |
                     (this.collectedBytes[0] & 0x3f);
      command &= 0x7fff;
      this.debug("EXT: " + command.toString(16));
      this.renderer = this.platoMode;
    }
  }

  platoModeLoadMemoryAddress(byte) {
    if (this.collectWord(byte)) {
      let address = ((this.collectedBytes[2] & 0x3f) << 12) |
                    ((this.collectedBytes[1] & 0x3f) <<  6) |
                     (this.collectedBytes[0] & 0x3f);
      this.loadMemoryAddress = address & 0xffff;
      this.debug("load memory address: " + this.loadMemoryAddress.toString(16));
      this.renderer = this.platoMode;
    }
  }

  platoModeLoadMemory(byte) {
    if (this.collectWord(byte)) {
      let word = ((this.collectedBytes[2] & 0x3f) << 12) |
                 ((this.collectedBytes[1] & 0x3f) <<  6) |
                  (this.collectedBytes[0] & 0x3f);
      word &= 0xffff;
      this.debug("load memory: <" + this.loadMemoryAddress.toString(16) + "> <- " + word.toString(16));
      this.loadMemoryAddress += 2;
      this.beginCollection();
    }
  }

  platoModeLoadCharset(byte) {
    if (this.collectWord(byte)) {
      let word = ((this.collectedBytes[2] & 0x3f) << 12) |
                 ((this.collectedBytes[1] & 0x3f) <<  6) |
                  (this.collectedBytes[0] & 0x3f);
      word &= 0xffff;
      if (this.loadMemoryAddress >= 0x3800 && this.loadMemoryAddress < 0x4000) {
        let wordIndex = (this.loadMemoryAddress - 0x3800) >> 1;
        let charIndex = wordIndex >> 3;
        let stripIndex = wordIndex & 7;
        this.alternateCharset[charIndex][stripIndex] = word;
        this.debug("load alternate character: <" + this.loadMemoryAddress.toString(16) + "> " +
          this.toHex(charIndex) + ":" + this.toHex(stripIndex) +
          " <- " + this.toHex(word >> 8) + this.toHex(word & 0xff));
      }
      else {
        this.debug("load alternate character: invalid address " + this.loadMemoryAddress.toString(16));
      }
      this.loadMemoryAddress += 2;
      this.beginCollection();
    }
  }

  platoModeLoadCoordinate(byte) {
    if (this.collectCoordinate(byte)) {
      let coord = this.computeCoordinate(this.collectedBytes);
      this.debug("load coordinate: (" + coord[0] + "," + coord[1] + ")");
      this.X = coord[0];
      this.Y = coord[1];
      this.renderer = this.platoMode;
    }
  }

  platoModePointPlot(byte) {
    if (this.collectCoordinate(byte)) {
      let coord = this.computeCoordinate(this.collectedBytes);
      this.beginCollection();
      this.drawPoint(coord[0], coord[1]);
    }
  }

  platoModeDrawLine(byte) {
    if (this.collectCoordinate(byte)) {
      let coord = this.computeCoordinate(this.collectedBytes);
      this.beginCollection();
      if (this.lastLineCoordinate) {
        this.drawLine(this.lastLineCoordinate[0], this.lastLineCoordinate[1], coord[0], coord[1]);
      }
      else {
        this.debug("begin line: (" + coord[0] + "," + coord[1] + ")");
        this.X = coord[0];
        this.Y = coord[1];
      }
      this.lastLineCoordinate = coord;
    }
  }

  renderText(text) {
    if (typeof text === "string") {
      for (let i = 0; i < text.length; i++) {
        this.renderer(text.charCodeAt(i) & 0x7f);
      }
    }
    else if (this.isDtCyberPterm) {
      // DtCyber implements partial Telnet protocol. It
      // escapes IAC (0xff) and adds 0 after 0x0d. Detect
      // these cases and remove them from the stream.
     for (let i = 0; i < text.byteLength; i++) {
       let b = text[i];
       switch (this.receiveState) {
       case this.RECEIVE_STATE_IAC:
         this.receiveState = this.RECEIVE_STATE_NORMAL;
         if (b !== 0xff) {
           this.renderer(b & 0x7f);
         }
         break;
       case this.RECEIVE_STATE_CR:
         if (b === 0) {
           this.receiveState = this.RECEIVE_STATE_NORMAL;
         }
         else {
           this.renderer(b & 0x7f);
           if (b !== 0x0d) {
             this.receiveState = this.RECEIVE_STATE_NORMAL;
           }
         }
         break;
       default:
         this.renderer(b & 0x7f);
         if (b === 0xff) {
           this.receiveState = this.RECEIVE_STATE_IAC;
         }
         else if (b === 0x0d) {
           this.receiveState = this.RECEIVE_STATE_CR;
         }
         break;
       }
     }
    }
    else {
      for (let i = 0; i < text.byteLength; i++) {
        this.renderer(text[i] & 0x7f);
      }
    }
  }
}
//
// The following lines enable this file to be used as a Node.js module.
//
if (typeof module === "undefined") module = {};
module.exports = PTerm;
