/*--------------------------------------------------------------------------
**
**  Copyright (c) 2017, Kevin Jordan
**
**  aterm.js
**    This class implements an ANSI X.364 (e.g., DEC VT-100) terminal emulator.
**
**--------------------------------------------------------------------------
*/

class ATerm {

  constructor() {

    this.autowrapMode  = false;
    this.bgndColor     = "black";
    this.canvas        = null;
    this.col           = 0;
    this.cols          = 80;
    this.context       = null;
    this.cursorKeyMode = false;
    this.escSeq        = null;
    this.fgndColor     = "lightgreen";
    this.fontHeight    = 16;
    this.fontWidth     = 10;
    this.inverse       = false;
    this.isScreenMode  = false;
    this.lfNewlineMode = false;
    this.originMode    = false;
    this.overstrike    = false;
    this.row           = 0;
    this.rows          = 24;
    this.tabStops      = [7,15,23,31,39,47,55,63,71,79];
    this.underline     = false;
    this.invertDelBs   = false;
    this.isKSR         = false;

    this.scrollingRegionBottom = this.rows - 1;
    this.scrollingRegionTop    = 0;

    this.ASCIIset      = [
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, // <NUL> - <SI>
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, // <DLE> - <US>
      0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, // <SP> - /
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, // 0 - ?
      0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, // @ - O
      0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, // P - _
      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, // ` - o
      0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f  // p - <DEL>
    ];

    this.graphicSet      = [
      0x00,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06,   0x07,   // <NUL> - <BEL>
      0x08,   0x09,   0x0a,   0x0b,   0x0c,   0x0d,   0x0e,   0x0f,   // <BS>  - <SI>
      0x10,   0x11,   0x12,   0x13,   0x14,   0x15,   0x16,   0x17,   // <DLE> - <ETB>
      0x18,   0x19,   0x1a,   0x1b,   0x1c,   0x1d,   0x1e,   0x1f,   // <CAN> - <US>
      0x20,   0x21,   0x22,   0x23,   0x24,   0x25,   0x26,   0x27,   // <SP> - '
      0x28,   0x29,   0x2a,   0x2b,   0x2c,   0x2d,   0x2e,   0x2f,   // ( - /
      0x30,   0x31,   0x32,   0x33,   0x34,   0x35,   0x36,   0x37,   // 0 - 7
      0x38,   0x39,   0x3a,   0x3b,   0x3c,   0x3d,   0x3e,   0x3f,   // 8 - ?
      0x40,   0x41,   0x42,   0x43,   0x44,   0x45,   0x46,   0x47,   // @ - G
      0x48,   0x49,   0x4a,   0x4b,   0x4c,   0x4d,   0x4e,   0x4f,   // H - O
      0x50,   0x51,   0x52,   0x53,   0x54,   0x55,   0x56,   0x57,   // P - W
      0x58,   0x59,   0x5a,   0x5b,   0x5c,   0x5d,   0x5e,   0x20,   // X - _
      0x25ca, 0x2591, 0x09,   0xc0,   0x0d,   0x0a,   0x00b0, 0x00b1, // ` - g
      0x0d,   0x0b,   0x2518, 0x2510, 0x250c, 0x2514, 0x253c, 0x2500, // h - o
      0x2500, 0x2500, 0x2500, 0x2500, 0x251c, 0x2524, 0x2534, 0x252c, // p - w
      0x2502, 0x2264, 0x2265, 0x220f, 0x2260, 0x00a3, 0x2022, 0x7f    // x - <DEL>
    ];

    this.APLset      = [
      0x00,   0x01,   0x02,   0x03,   0x04,   0x05,   0x06,   0x07,   // <NUL> - <BEL>
      0x08,   0x09,   0x0a,   0x0b,   0x0c,   0x0d,   0x0e,   0x0f,   // <BS>  - <SI>
      0x10,   0x11,   0x12,   0x13,   0x14,   0x15,   0x16,   0x17,   // <DLE> - <ETB>
      0x18,   0x19,   0x1a,   0x1b,   0x1c,   0x1d,   0x1e,   0x1f,   // <CAN> - <US>
      0x20,   0xf0a8, 0xf029, 0xf03c, 0xf088, 0xf03d, 0xf03e, 0xf05d, // <SP> - '
      0xf09f, 0xf05e, 0xf0ac, 0xf0f7, 0xf02c, 0xf02b, 0xf02e, 0xf02f, // ( - /
      0xf030, 0xf031, 0xf032, 0xf033, 0xf034, 0xf035, 0xf036, 0xf037, // 0 - 7
      0xf038, 0xf039, 0xf028, 0xf05b, 0xf03b, 0xf0d7, 0xf03a, 0xf05c, // 8 - ?
      0xf0af, 0xf0b8, 0xf083, 0xf09d, 0xf098, 0xf0b9, 0xf05f, 0xf092, // @ - G
      0xf091, 0xf0bc, 0xf0b0, 0xf027, 0xf08c, 0xf07c, 0xf082, 0xf0b1, // H - O
      0xf02a, 0xf03f, 0xf0bd, 0xf097, 0xf07e, 0xf087, 0xf09e, 0xf0be, // P - W
      0xf09c, 0xf086, 0xf09b, 0xf084, 0xf0a4, 0xf085, 0xf089, 0xf02d, // X - _
      0xf0aa, 0xf041, 0xf042, 0xf043, 0xf044, 0xf045, 0xf046, 0xf047, // ` - g
      0xf048, 0xf049, 0xf04a, 0xf04b, 0xf04c, 0xf04d, 0xf04e, 0xf04f, // h - o
      0xf050, 0xf051, 0xf052, 0xf053, 0xf054, 0xf055, 0xf056, 0xf057, // p - w
      0xf058, 0xf059, 0xf05a, 0xf07b, 0xf081, 0xf07d, 0xf024, 0x7f    // x - <DEL>
    ];

    this.normalSet     = this.ASCIIset;
    this.charset       = this.normalSet;
    this.G0            = this.normalSet;
    this.G1            = this.normalSet;
  }

  setFont(type) {
    switch (type.toLowerCase()) {
    case "apl":
      this.charset = this.normalSet = this.APLset;
      this.font = `${this.fontHeight - 1}px APL2741`;
      this.overstrike = true;
      break;
    default:
      this.charset = this.normalSet = this.ASCIIset;
      this.font = `${this.fontHeight - 1}px Courier New`;
      this.overstrike = false;
      break;
    }
    this.G0 = this.G1 = this.normalSet;

    let testLine = "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM";
    let cnv = document.createElement("canvas");
    let ctx = cnv.getContext("2d");
    cnv.width = testLine.length * 15;
    cnv.height = this.fontHeight;
    ctx.font = this.font;
    this.fontWidth = Math.round(ctx.measureText(testLine).width / testLine.length);
  }

  setInvertDelBs(invertDelBs) {
    this.invertDelBs = invertDelBs;
  }

  setIsKSR(isKSR) {
    this.isKSR = isKSR;
  }

  setUplineDataSender(callback) {
    this.uplineDataSender = callback;
  }

  setWidthChangeListener(callback) {
    this.widthChangeListener = callback;
  }

  changeFont(type) {
    this.hideCursor();
    let prevScreen = this.context.getImageData(0, 0, this.cols * this.fontWidth, this.rows * this.fontHeight);
    this.setFont(type);
    this.canvas.width = this.fontWidth * this.cols;
    this.canvas.height = this.fontHeight * this.rows;
    this.context.font = this.font; // restore font after width/height changed
    this.context.textBaseline = "top";
    this.context.fillStyle = this.bgndColor;
    this.context.clearRect(0, 0, this.canvas.width, this.canvas.height);
    this.context.fillRect(0, 0, this.canvas.width, this.canvas.height);
    this.context.putImageData(prevScreen, 0, 0);
    this.showCursor();
  }

  getCanvas() {
    return this.canvas;
  }

  getWidth() {
    return this.fontWidth * this.cols;
  }

  changeWidth(cols) {
    this.hideCursor();
    let prevScreen = this.context.getImageData(0, 0, this.cols * this.fontWidth, this.rows * this.fontHeight);
    this.cols = cols;
    let width = this.fontWidth * this.cols;
    let height= this.rows * this.fontHeight;
    this.canvas.width = width;
    this.context.font = this.font; // restore font after width/height changed
    this.context.textBaseline = "top";
    this.context.fillStyle = this.bgndColor;
    this.context.clearRect(0, 0, width, height);
    this.context.fillRect(0, 0, width, height);
    this.context.putImageData(prevScreen, 0, 0);
    this.showCursor();
    if (typeof this.widthChangeListener === "function") {
      this.widthChangeListener(width);
    }
  }

  processKeyboardEvent(keyStr, shiftKey, ctrlKey, altKey) {
    let sendStr = "";
    //
    // Handle non-special keys
    //
    if (keyStr.length < 2) { // non-special key
      if (ctrlKey) {
        sendStr = String.fromCharCode(keyStr.charCodeAt(0) & 0x1f);
      }
      else {
        sendStr = keyStr;
      }
      if (sendStr === "\r" && this.lfNewlineMode) {
        sendStr = "\r\n";
      }
    }
    //
    // Handle special keys
    //
    else {

      switch (keyStr) {
      case "Backspace":
        if (this.invertDelBs) {
          sendStr = "\x7F";
        }
        else {
          sendStr = "\b";
        }
        break;
      case "Delete":
        if (this.invertDelBs) {
          sendStr = "\b";
        }
        else {
          sendStr = "\x7F";
        }
        break;
      case "Enter":
        if (this.lfNewlineMode) {
          sendStr = "\r\n";
        }
        else {
          sendStr = "\r";
        }
        break;
      case "Escape":
        sendStr = "\x1B";
        break;
      case "Tab":
        sendStr = "\t";
        break;
      case "ArrowDown":
        sendStr = this.cursorKeyMode ? "\x1BOB" : "\x1B[B";
        break;
      case "ArrowLeft":
        sendStr = this.cursorKeyMode ? "\x1BOD" : "\x1B[D";
        break;
      case "ArrowRight":
        sendStr = this.cursorKeyMode ? "\x1BOC" : "\x1B[C";
        break;
      case "ArrowUp":
        sendStr = this.cursorKeyMode ? "\x1BOA" : "\x1B[A";
        break;
      case "F1":
        if (altKey) { // VT100 PF1
          sendStr = "\x1BOP";
        }
        else {
          sendStr = shiftKey ? "\x1BOP\r" : "\x1BOq\r";
        }
        break;
      case "F2":
        if (altKey) { // VT100 PF2
          sendStr = "\x1BOQ";
        }
        else {
          sendStr = shiftKey ? "\x1BOQ\r" : "\x1BOr\r";
        }
        break;
      case "F3":
        if (altKey) { // VT100 PF3
          sendStr = "\x1BOR";
        }
        else {
          sendStr = shiftKey ? "\x1BOR\r" : "\x1BOs\r";
        }
        break;
      case "F4":
        if (altKey) { // VT100 PF4
          sendStr = "\x1BOS";
        }
        else {
          sendStr = shiftKey ? "\x1BOS\r" : "\x1BOt\r";
        }
        break;
      case "F5":
        sendStr = shiftKey ? "\x1BOm\r" : "\x1BOu\r";
        break;
      case "F6":
        sendStr = shiftKey ? "\x1BOl\r" : "\x1BOv\r";
        break;
      case "F7":
        sendStr = shiftKey ? "\x1BOM\r" : "\x1BOw\r";
        break;
      case "F8":
        sendStr = shiftKey ? "\x1BOn\r" : "\x1BOx\r";
        break;
      case "F9":
        sendStr = shiftKey ? "\x1BOp\r" : "\x1BOy\r";
        break;
      case "F10":
        sendStr = shiftKey ? "\x1BOT\r" : "\x1BOz\r";
        break;
      case "F11":
        break;
      case "F12":
        break;
      default: // ignore the key
        break;
      }
    }
    if (this.uplineDataSender) {
      this.uplineDataSender(sendStr);
    }
  }

  createScreen(fontType, rows, cols) {
    this.rows = rows;
    this.cols = cols;
    this.canvas = document.createElement("canvas");
    this.context = this.canvas.getContext("2d");
    this.setFont(fontType);
    this.canvas.width = this.fontWidth * this.cols;
    this.canvas.height = this.fontHeight * this.rows;
    this.context.font = this.font; // restore font after width/height changed
    this.context.textBaseline = "top";
    this.canvas.style.border = "1px solid black";
    this.reset();
    this.canvas.setAttribute("tabindex", 0);
    this.canvas.setAttribute("contenteditable", "true");
    this.showCursor();

    const me = this;

    this.canvas.addEventListener("click", () => {
      me.canvas.focus();
    });

    this.canvas.addEventListener("mouseover", () => {
      me.canvas.focus();
    });

    this.canvas.addEventListener("keydown", evt => {
      evt.preventDefault();
      me.processKeyboardEvent(evt.key, evt.shiftKey, evt.ctrlKey, evt.altKey);
    });
  }

  reset() {
    this.autowrapMode  = false;
    this.row           = 0;
    this.col           = 0;
    this.charset       = this.normalSet;
    this.cursorKeyMode = false;
    this.bgndColor     = "black";
    this.escSeq        = null;
    this.fgndColor     = "lightgreen";
    this.G0            = this.normalSet;
    this.G1            = this.normalSet;
    this.inverse       = false;
    this.lfNewlineMode = false;
    this.isScreenMode  = false;
    this.originMode    = false;
    this.scrollingRegionBottom = this.rows - 1;
    this.scrollingRegionTop    = 0;
    this.tabStops      = [7,15,23,31,39,47,55,63,71,79];
    this.underline     = false;
    let width = this.cols * this.fontWidth;
    let height = this.rows * this.fontHeight;
    this.context.fillStyle = this.bgndColor;
    this.context.clearRect(0, 0, width, height);
    this.context.fillRect(0, 0, width, height);
  }

  invertScreen() {
    let imageData = this.context.getImageData(0, 0, this.cols * this.fontWidth, this.rows * this.fontHeight);
    let data = imageData.data;
    for (let i = 0; i < data.length; i += 4) {
      if (data[i] !== 0) { // black is #000000
        data[i] = data[i + 1] = data[i + 2] = 0;
      }
      else { // lightgreen is #90EE90
        data[i]     = 0x90;
        data[i + 1] = 0xEE;
        data[i + 2] = 0x90;
      }
//      data[i] = 255 - data[i];          // invert R of RGB
//      data[i + 1] = 255 - data[i + 1];  // invert G of RGB
//      data[i + 2] = 255 - data[i + 2];  // invert B of RGB
    }
    this.context.putImageData(imageData, 0, 0);
  }

  drawChar(ch, row, col) {
    if (row < this.rows && col < this.cols) {
      let x = col * this.fontWidth;
      let y = row * this.fontHeight
      let fgndColor = this.inverse ? this.bgndColor : this.fgndColor;
      let bgndColor = this.inverse ? this.fgndColor : this.bgndColor;
      if (!this.overstrike) {
        this.context.fillStyle = bgndColor;
        this.context.clearRect(x, y, this.fontWidth, this.fontHeight);
        this.context.fillRect(x, y, this.fontWidth, this.fontHeight);
      }
      this.context.fillStyle = fgndColor;
      this.context.fillText(ch, x, y);
      if (this.underline) {
        y = y + this.fontHeight - 1;
        this.context.moveTo(x, y);
        this.context.lineTo(x + this.fontWidth - 1, y);
        this.context.strokeStyle = fgndColor;
        this.context.stroke();
      }
    }
  }

  clearRegion(firstRow, firstCol, rows, cols) {
    if (rows > 0 && cols > 0) {
      let y = firstRow * this.fontHeight;
      let x = firstCol * this.fontWidth;
      let width = cols * this.fontWidth;
      let height = rows * this.fontHeight;
      this.context.fillStyle = this.inverse ? this.fgndColor : this.bgndColor;
      this.context.clearRect(x, y, width, height);
      this.context.fillRect(x, y, width, height);
    }
  }

  showCursor() {
    if (this.row < this.rows && this.col < this.cols) {
      this.cursorX = this.col * this.fontWidth;
      this.cursorY = this.row * this.fontHeight;
      this.cursorCharBlock = this.context.getImageData(this.cursorX, this.cursorY, this.fontWidth, this.fontHeight);
      this.context.fillStyle = this.inverse ? this.bgndColor : this.fgndColor;
      this.context.fillRect(this.cursorX, this.cursorY, this.fontWidth, this.fontHeight);
    }
    else if (this.cursorCharBlock) {
      delete this.cursorCharBlock;
    }
  }

  hideCursor() {
    if (this.cursorCharBlock) {
      this.context.putImageData(this.cursorCharBlock, this.cursorX, this.cursorY);
      delete this.cursorCharBlock;
    }
  }

  renderText(text) {

    let p1, p2, params;

    if (typeof text === "string") {
      let ab = new Uint8Array(text.length);
      for (let i = 0; i < text.length; i++) {
        ab[i] = text.charCodeAt(i) & 0xff;
      }
      text = ab;
    }

    this.hideCursor();

    for (let i = 0; i < text.byteLength; i++) {

      let chCode = text[i] & 0x7f;
      let ch = String.fromCharCode(this.charset[chCode]);

      //
      // If this.escSeq is null, collection of an escape sequence
      // is not in progress, so process a normal character.
      //

      if (this.escSeq === null) {
        switch (ch) {
        case "\x00":
        case "\x01":
        case "\x02":
        case "\x03":
        case "\x04":
        case "\x05":
        case "\x06":
        case "\x07":
          break;
        case "\x08": // <BS>
          if (this.col > 0) { --this.col; }
          break;
        case "\x09": // <HT>
          let t = 0;
          while (t < this.tabStops.length) {
            if (this.tabStops[t] > this.col) {
              this.col = this.tabStops[t];
              break;
            }
            t++;
          }
          if (t >= this.tabStops.length) { this.col = this.cols - 1; }
          break;
        case "\x0A": // <LF>
          if (this.lfNewlineMode) {
            this.col = 0;
          }
          if (this.row === this.scrollingRegionBottom) {
            this.scrollUp(this.scrollingRegionTop, this.row);
          }
          else if (this.row >= this.rows) {
            this.row = this.rows - 1;
            this.scrollUp(0, this.row);
          }
          else {
            this.row++;
          }
          break;
        case "\x0B":
        case "\x0C":
          break;
        case "\x0D": // <CR>
          this.col = 0;
          break;
        case "\x0E": // <SO>
          this.charset = this.G1; // select G1 character set
          break;
        case "\x0F": // <SI>
          this.charset = this.G0; // select G0 character set
          break;
        case "\x10":
        case "\x11":
        case "\x12":
        case "\x13":
        case "\x14":
        case "\x15":
        case "\x16":
        case "\x17":
        case "\x18":
        case "\x19":
        case "\x1A":
          break;
        case "\x1B": // <ESC>
          this.escSeq = ch;
          break;
        case "\x1C":
        case "\x1D":
        case "\x1E":
        case "\x1F":
        case "\x7F":
          break;
        default:
          if (this.col >= this.cols) {
            if (this.autowrapMode) {
              if (this.row === this.scrollingRegionBottom) {
                this.scrollUp(this.scrollingRegionTop, this.row);
              }
              else if (this.row >= this.rows) {
                this.row = this.rows - 1;
                this.scrollUp(0, this.row);
              }
              else {
                this.row++;
              }
              this.col = 0;
            }
            else {
              this.col = this.cols - 1;
            }
          }
          this.drawChar(ch, this.row, this.col++);
          break;
        }
      }

      //
      // If this.escSeq is not null, an escape sequence is being
      // collected.
      //

      else {

        this.escSeq += ch;

        switch (this.escSeq.charAt(1)) {
        case "[":
          let len = this.escSeq.length;
          if (len > 2 && "0123456789;?".indexOf(ch) < 0) {
            params = [];
            if (len > 3) {
              this.escSeq.substr(2, len - 3).split(";").forEach(p => {
                if (p.startsWith("?")) {
                  // Indicate DEC private parameter by negating the value
                  params.push(0 - parseInt(p.substr(1)));
                }
                else if (p.length > 0) {
                  params.push(parseInt(p));
                }
                else {
                  params.push(0);
                }
              });
            }
            switch (ch) {
            case "A": // (CUP) Cursor Up
              p1 = (params.length > 0) ? params.shift() : 1;
              if (p1 < 1) p1 = 1;
              if (this.row >= this.scrollingRegionTop && this.row <= this.scrollingRegionBottom) {
                this.row -= p1;
                if (this.row < this.scrollingRegionTop) {
                  this.row = this.scrollingRegionTop;
                }
              }
              else {
                this.row -= p1;
                if (this.row < 0) {
                  this.row = 0;
                }
              }
              break;
            case "B": // (CUD) Cursor Down
              p1 = (params.length > 0) ? params.shift() : 1;
              if (p1 < 1) p1 = 1;
              if (this.row >= this.scrollingRegionTop && this.row <= this.scrollingRegionBottom) {
                this.row += p1;
                if (this.row > this.scrollingRegionBottom) {
                  this.row = this.scrollingRegionBottom;
                }
              }
              else {
                this.row += p1;
                if (this.row >= this.rows) {
                  this.row = this.rows - 1;
                }
              }
              break;
            case "C": // (CUF) Cursor Forward
              p1 = (params.length > 0) ? params.shift() : 1;
              if (p1 < 1) p1 = 1;
              this.col += p1;
              if (this.col >= this.cols) this.col = this.cols - 1;
              break;
            case "D": // (CUB) Cursor Backward
              p1 = (params.length > 0) ? params.shift() : 1;
              if (p1 < 1) p1 = 1;
              this.col -= p1;
              if (this.col < 0) this.col = 0;
              break;
            case "H": // (CUP) Cursor Position
            case "f": // (HVP) Horizontal and Vertical Position
              this.row = this.scrollingRegionTop;
              this.col = 0;
              if (params.length > 0) {
                this.row = params.shift();
                if (this.row > 0) {
                  this.row--;
                }
                else {
                  this.row = 0;
                }
                if (this.originMode) {
                  this.row += this.scrollingRegionTop;
                  if (this.row > this.scrollingRegionBottom) {
                    this.row = this.scrollingRegionBottom;
                  }
                }
                else if (this.row >= this.rows) {
                  this.row = this.rows - 1;
                }
              }
              if (params.length > 0) {
                this.col = params.shift();
                if (this.col > 0) {
                  --this.col;
                  if (this.col >= this.cols) this.col = this.cols - 1;
                }
                else {
                  this.col = 0;
                }
              }
              break;
            case "J": // (ED) Erase In Display
              p1 = (params.length > 0) ? params.shift() : 0;
              switch (p1) {
              case 0: // Erase from the active position to the end of the screen, inclusive
                this.clearRegion(this.row, this.col, 1, this.cols - this.col);
                this.clearRegion(this.row + 1, 0, this.rows - (this.row + 1), this.cols);
                break;
              case 1: // Erase from start of the screen to the active position, inclusive
                this.clearRegion(0, 0, this.row, this.cols);
                this.clearRegion(this.row, 0, 1, this.col + 1);
                break;
              case 2: // Erase all of the display.
                this.clearRegion(0, 0, this.rows, this.cols);
                break;
              }
              break;
            case "K": // (EL) Erase In Line
              p1 = (params.length > 0) ? params.shift() : 0;
              switch (p1) {
              case 0: // Erase from the active position to the end of the line, inclusive
                this.clearRegion(this.row, this.col, 1, this.cols - this.col);
                break;
              case 1: // Erase from start of the line to the active position, inclusive
                this.clearRegion(this.row, 0, 1, this.col + 1);
                break;
              case 2: // Erase all of the line.
                this.clearRegion(this.row, 0, 1, this.cols);
                break;
              }
              break;
            case "c": // What Are You?
              if (this.uplineDataSender) {
                this.uplineDataSender("\x1B[?1;0c");
              }
              break;
            case "h": // (SM) Set Mode
              for (let i = 0; i < params.length; i++) {
                // set the indicated mode
                // ?1 - Cursor key mode
                // ?2 - VT52
                // ?3 - Column (132 vs 80)
                // ?4 - Scrolling
                // ?5 - Screen
                // ?6 - Origin
                // ?7 - Autowrap
                // ?8 - Auto repeat
                // ?9 - Interlace
                // 20 - Line feed new line mode
                switch (params[i]) {
                case -3:
                  this.changeWidth(132);
                  this.clearRegion(0, 0, this.rows, this.cols);
                  this.row = this.col = 0;
                  this.scrollingRegionTop = 0;
                  this.scrollingRegionBottom = this.rows - 1;
                  break;
                case -5:
                  if (this.isScreenMode === false) {
                    this.bgndColor = "lightgreen";
                    this.fgndColor = "black";
                    this.isScreenMode = true;
                    this.invertScreen();
                  }
                  break;
                case -6:
                  this.originMode = true;
                  this.row = this.scrollingRegionTop;
                  this.col = 0;
                  break;
                case -7:
                  this.autowrapMode = true;
                  break;
                case 20:
                  this.lfNewlineMode = true;
                  break;
                }
              }
              break;
            case "g": // (TBC) Tabulation Clear
              if (params.length < 1) params.push(0);
              switch (params.shift()) {
              case 0: // Clear tab stop at active position
                for (let t = 0; t < this.tabStops.length; t++) {
                  if (this.tabStops[t] === this.col) {
                    this.tabStops.splice(t, 1);
                    break;
                  }
                }
                break;
              case 3: // Clear all tab stops
                this.tabStops = [];
                break;
              }
              break;
            case "l": // (RM) Reset Mode
              for (let i = 0; i < params.length; i++) {
                switch (params[i]) {
                case -3:
                  this.changeWidth(80);
                  this.clearRegion(0, 0, this.rows, this.cols);
                  this.row = this.col = 0;
                  this.scrollingRegionTop = 0;
                  this.scrollingRegionBottom = this.rows - 1;
                  break;
                case -5:
                  if (this.isScreenMode === true) {
                    this.bgndColor = "black";
                    this.fgndColor = "lightgreen";
                    this.isScreenMode = false;
                    this.invertScreen();
                  }
                  break;
                case -6:
                  this.originMode = false;
                  this.row = this.col = 0;
                  break;
                case -7:
                  this.autowrapMode = false;
                  break;
                case 20:
                  this.lfNewlineMode = false;
                  break;
                }
              }
              break;
            case "m": // (SGR) Select Graphic Rendition
              if (params.length < 1) params.push(0);
              for (let i = 0; i < params.length; i++) {
                switch (params[i]) {
                case 0: // attributes off
                  this.underline = this.inverse = false;
                  break;
                case 1: // Bold
                  break;
                case 4: // Underscore
                  this.underline = true;
                  break;
                case 5: // Blink
                  break;
                case 7: // Negative (inverse)
                  this.inverse = true;
                  break;
                }
              }
              break;
            case "n": // (DSR) Device Status Report
              if (params.length > 0 && this.uplineDataSender) {
                switch (params[0]) {
                case 5: // please report status
                  this.uplineDataSender("\x1B[0n");
                  break;
                case 6: // please report active position
                  this.uplineDataSender(`\x1B[${(this.row - this.scrollingRegionTop) + 1};${this.col + 1}R`);
                  break;
                }
              }
              break;
            case "r": // (DECSTBM) Scrolling Region
              // p1 - top line of region
              // p2 - bottom line of region
              this.scrollingRegionTop = 0;
              this.scrollingRegionBottom = this.rows - 1;
              if (params.length > 0) {
                this.scrollingRegionTop = params[0];
                if (this.scrollingRegionTop > 0) {
                  this.scrollingRegionTop--;
                }
                if (params.length > 1) {
                  this.scrollingRegionBottom = params[1];
                  if (this.scrollingRegionBottom < 1) {
                    this.scrollingRegionBottom = this.rows - 1;
                  }
                  else {
                    this.scrollingRegionBottom--;
                  }
                }
              }
              if (this.scrollingRegionTop >= this.scrollingRegionBottom) {
                this.scrollingRegionBottom = this.scrollingRegionTop + 1;
              }
              this.row = this.originMode ? this.scrollingRegionTop : 0;
              this.col = 0;
              break;
            case "x": // (DECREQTPARM) Request Terminal Parameters
              // 0 or none - unsolicited reports enabled
              // 1 - report on explicit request only
              // 2 - this message is a report
              // 3 - this message is a report triggered by explicit request
              break;
            case "y": // (DECTST) Invoke Confidence Test
              // p1 == 2
              // p2 == 0 - reset terminal
              if (params.length < 1 || params.shift() == 0) {
                this.reset();
              }
              break;
            }
            this.escSeq = null;
          }
          break;
        case "#":
          if (this.escSeq.length > 2) {
            switch (this.escSeq.charAt(2)) {
            case "3": // Change this line to double-height top half
            case "4": // Change this line to double-height bottom half
            case "5": // Change this line to single-width single-height
            case "6": // Change this line to double-width single-height
              break;
            case "8": // Screen Alignment Display
              for (let r = 0; r < this.rows; r++) {
                for (let c = 0; c < this.cols; c++) {
                  this.drawChar("E", r, c);
                }
              }
              break;
            }
            this.escSeq = null;
          }
          break;
        case "(": // Set G0 character set, invoked by <SI>
          if (this.escSeq.length > 2) {
            switch (this.escSeq.charAt(2)) {
            case "A": // UK set
              break;
            case "B": // ASCII set
              this.G0 = this.normalSet;
              break;
            case "0": // Special graphics
              this.G0 = this.graphicSet;
              break;
            case "1": // Alternate character ROM standard character set
              break;
            case "2": // Alternate character ROM special character set
              break;
            default:
              break;
            }
            this.escSeq = null;
          }
          break;
        case ")": // Set G1 character set, invoked by <SO>
          if (this.escSeq.length > 2) {
            switch (this.escSeq.charAt(2)) {
            case "A": // UK set
              break;
            case "B": // ASCII set
              this.G1 = this.normalSet;
              break;
            case "0": // Special graphics
              this.G1 = this.graphicSet;
              break;
            case "1": // Alternate character ROM standard character set
              break;
            case "2": // Alternate character ROM special character set
              break;
            default:
              break;
            }
            this.escSeq = null;
          }
          break;
        case "=": // Keypad Application Mode
          this.escSeq = null;
          break;
        case ">": // Keypad Numeric Mode
          this.escSeq = null;
          break;
        case "7": // Save Cursor
          this.savedRow = this.row;
          this.savedCol = this.col;
          this.escSeq = null;
          break;
        case "8": // Restore Cursor
          if (this.savedRow && this.savedCol) {
            this.row = this.savedRow;
            this.col = this.savedCol;
          }
          this.escSeq = null;
          break;
        case "D": // Cursor Down
          if (this.row === this.scrollingRegionBottom) {
            this.scrollUp(this.scrollingRegionTop, this.row);
          }
          else if (this.row >= this.rows) {
            this.row = this.rows - 1;
            this.scrollUp(0, this.row);
          }
          else {
            this.row++;
          }
          this.escSeq = null;
          break;
        case "E": // Next Line
          if (this.row === this.scrollingRegionBottom) {
            this.scrollUp(this.scrollingRegionTop, this.row);
          }
          else if (this.row >= this.rows) {
            this.row = this.rows - 1;
            this.scrollUp(0, this.row);
          }
          else {
            this.row++;
          }
          this.col = 0;
          this.escSeq = null;
          break;
        case "H": // Set Horizontal Tab
          this.tabStops.push(this.col);
          this.tabStops.sort((a, b) => a - b);
          this.escSeq = null;
          break;
        case "M": // Cursor Up
          if (this.row === this.scrollingRegionTop) {
            this.scrollDown(this.row, this.scrollingRegionBottom);
          }
          else if (this.row < 1) {
            this.row = 0;
            this.scrollDown(this.row, this.rows - 1);
          }
          else {
            this.row--;
          }
          this.escSeq = null;
          break;
        case "Z": // Identify Terminal
          this.escSeq = null;
          break;
        case "c": // Reset Terminal
          this.reset();
          this.escSeq = null;
          break;
        default:
          this.escSeq = null;
          break;
        }
      }
    }

    this.showCursor();
  }

  scrollUp(topRow, bottomRow) {
    let w = this.cols * this.fontWidth;
    let h = (bottomRow - topRow) * this.fontHeight;
    let y = topRow * this.fontHeight;
    let charBlock = this.context.getImageData(0, y + this.fontHeight, w, h);
    this.context.putImageData(charBlock, 0, y);
    this.context.fillStyle = this.bgndColor;
    this.context.clearRect(0, y + h, w, this.fontHeight);
    this.context.fillRect(0, y + h, w, this.fontHeight);
  }

  scrollDown(topRow, bottomRow) {
    let w = this.cols * this.fontWidth;
    let h = (bottomRow - topRow) * this.fontHeight;
    let y = topRow * this.fontHeight;
    let charBlock = this.context.getImageData(0, y, w, h);
    this.context.putImageData(charBlock, 0, y + this.fontHeight);
    this.context.fillStyle = this.bgndColor;
    this.context.clearRect(0, y, w, this.fontHeight);
    this.context.fillRect(0, y, w, this.fontHeight);
  }
}
//
// The following lines enable this file to be used as a Node.js module.
//
if (typeof module === "undefined") module = {};
module.exports = ATerm;
