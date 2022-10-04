/*--------------------------------------------------------------------------
**
**  Copyright (c) 2019, Kevin Jordan
**
**  tekterm.js
**    This class implements an Tektronix 4010 terminal emulator.
**
**--------------------------------------------------------------------------
*/

class Tektronix {

  constructor() {

    //
    // Terminal screen dimensions
    //
    this.SCREEN_HEIGHT = 780;
    this.SCREEN_WIDTH  = 1024;

    //
    // Terminal states
    //
    this.ST_NORMAL     = 0;
    this.ST_ESCAPE     = 1;

    //
    // Map of font heights by screen dimenions
    //
    this.fontHeightMap = {
      "35x74":  22,
      "38x81":  20,
      "58x121": 13,
      "64x133": 10
    };

    //
    // Character set tables
    //
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

    //
    // Terminal emulation properties
    //
    this.aplFontFamily     = "APL2741";
    this.asciiFontFamily   = "Courier New";
    this.bgndColor         = "darkgreen";
    this.canvas            = null;
    this.charset           = this.ASCIIset;
    this.context           = null;
    this.fgndColor         = "lightyellow";
    this.fontType          = "normal";
    this.invertDelBs       = false;
    this.isKSR             = false;
    this.state             = this.ST_NORMAL;
  }

  setFont(fontType) {
    if (fontType === "apl") {
      this.fontFamily = this.aplFontFamily;
      this.charset = this.APLset;
    }
    else {
      this.fontFamily = this.asciiFontFamily;
      this.charset = this.ASCIIset;
    }
    this.fontHeight = this.fontHeightMap[this.screenDims];
    let font = `${this.fontHeight}px ${this.fontFamily}`;
    this.context.font = font;
    let testLine = "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM";
    let cnv = document.createElement("canvas");
    let ctx = cnv.getContext("2d");
    cnv.width = testLine.length * 15;
    cnv.height = this.fontHeight;
    ctx.font = font;
    this.fontWidth = Math.round(ctx.measureText(testLine).width / testLine.length);
  }

  changeFont(type) {
    this.hideCursor();
    this.setFont(type);
    this.showCursor();
  }

  setKeyboardInputHandler(callback) {
    this.keyboardInputHandler = callback;
  }

  setInvertDelBs(invertDelBs) {
    this.invertDelBs = invertDelBs;
  }

  setIsKSR(isKSR) {
    this.isKSR = isKSR;
  }

  getCanvas() {
    return this.canvas;
  }

  getWidth() {
    return this.canvas.width;
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
        sendStr = "\r";
        break;
      case "Escape":
        sendStr = "\x1B";
        break;
      case "Tab":
        sendStr = "\t";
        break;
      case "ArrowDown":
        sendStr = "\x0A";
        break;
      case "ArrowLeft":
        sendStr = "\b";
        break;
      case "ArrowRight":
        sendStr = " ";
        break;
      case "ArrowUp":
        sendStr = "\x0B";
        break;
      case "F1":
        if (shiftKey) {
          //
          // RESET - reset terminal to initial status, but do not erase screen
          //
          this.hideCursor();
          this.reset();
          this.showCursor();
        }
        else {
          //
          // PAGE - erase screen, reset to Alpha mode and home position,
          //        reset to margin 1, cancel Bypass condition
          //
          this.x            = 0;
          this.y            = this.SCREEN_HEIGHT - this.fontHeight;
          this.marginOffset = 0;
          this.isGraphMode  = false;
          this.isBypass     = false;
          this.clearScreen();
          this.showCursor();
        }
        break;
      case "F2":
      case "F3":
      case "F4":
      case "F5":
      case "F6":
      case "F7":
      case "F8":
      case "F9":
      case "F10":
      case "F11":
      case "F12":
        break;
      default: // ignore the key
        break;
      }
    }
    if (this.isGINMode) {
      this.hideCrosshairs();
      this.isGINMode = false;
      sendStr += String.fromCharCode((this.crosshairX >> 5)   | 0x40)
              +  String.fromCharCode((this.crosshairX & 0x1f) | 0x40)
              +  String.fromCharCode((this.crosshairY >> 5)   | 0x40)
              +  String.fromCharCode((this.crosshairY & 0x1f) | 0x40);
    }
    if (this.keyboardInputHandler) {
      this.keyboardInputHandler(sendStr);
    }
  }

  createScreen() {
    this.canvas = document.createElement("canvas");
    this.context = this.canvas.getContext("2d");
    this.canvas.width = this.SCREEN_WIDTH;
    this.canvas.height = this.SCREEN_HEIGHT;
    this.canvas.style.cursor = "default";
    this.crosshairX  = 0;
    this.crosshairY  = this.SCREEN_HEIGHT - 1;
    this.isGINMode   = false;
    this.isGraphMode = false;
    this.isIgnoreCR  = false;
    this.isBypass    = false;
    this.hiY         = 0;
    this.loY         = 0;
    this.hiX         = 0;
    this.loX         = 0;
    this.reset();
    this.clearScreen();
    this.context.textBaseline = "top";
    this.canvas.style.border = "1px solid black";
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

    this.canvas.addEventListener("mousemove", evt => {
      let rect = me.canvas.getBoundingClientRect();
      me.crosshairX = evt.clientX - rect.left;
      me.crosshairY = (me.SCREEN_HEIGHT - 1) - (evt.clientY - rect.top);
    });

    this.canvas.addEventListener("keydown", evt => {
      evt.preventDefault();
      me.processKeyboardEvent(evt.key, evt.shiftKey, evt.ctrlKey, evt.altKey);
    });
  }

  clearScreen() {
    this.context.fillStyle = this.bgndColor;
    this.context.clearRect(0, 0, this.SCREEN_WIDTH, this.SCREEN_HEIGHT);
    this.context.fillRect(0, 0, this.SCREEN_WIDTH, this.SCREEN_HEIGHT);
  }

  reset() {
    this.bgndColor    = "green";
    this.fgndColor    = "white";
    this.fontType     = "normal";
    this.screenDims   = "35x74";
    this.setFont("normal");
    this.x            = 0;
    this.y            = this.SCREEN_HEIGHT - this.fontHeight;
    this.marginOffset = 0;
    this.isGraphMode  = false;
    this.isBypass     = false;
    if (this.isGINMode) {
      this.hideCrosshairs();
      this.isGINMode    = false;
    }
    this.isIgnoreCR   = false;
  }

  drawChar(b) {
    let cursorY = ((this.SCREEN_HEIGHT - 1) - this.y) - (this.fontHeight - 1);
//  this.context.fillStyle = this.bgndColor;
//  this.context.clearRect(this.x, cursorY, this.fontWidth, this.fontHeight);
//  this.context.fillRect(this.x, cursorY, this.fontWidth, this.fontHeight);
    this.context.fillStyle = this.fgndColor;
    this.context.fillText(String.fromCharCode(this.charset[b & 0x7f]), this.x, cursorY);
  }

  drawVector(x1, y1, x2, y2) {
    if (y1 >= this.SCREEN_HEIGHT || y2 >= this.SCREEN_HEIGHT) {
      //
      // Calculate point where line intersects y = SCREEN_HEIGHT - 1
      //
      let m = (y2 - y1) / (x2 - x1);
      let b = y2 - (m * x2);
      let x = Math.round(((this.SCREEN_HEIGHT - 1) - b) / m);
      if (y1 < this.SCREEN_HEIGHT) {
        this.context.beginPath();
        this.context.moveTo(x1, (this.SCREEN_HEIGHT - 1) - y1);
        this.context.lineTo(x, this.SCREEN_HEIGHT - 1);
        this.context.strokeStyle = this.fgndColor;
        this.context.stroke();
      }
      else if (y2 < this.SCREEN_HEIGHT) {
        this.context.beginPath();
        this.context.moveTo(x, this.SCREEN_HEIGHT - 1);
        this.context.lineTo(x2, (this.SCREEN_HEIGHT - 1) - y2);
        this.context.strokeStyle = this.fgndColor;
        this.context.stroke();
      }
    }
    else {
      this.context.beginPath();
      this.context.moveTo(x1, (this.SCREEN_HEIGHT - 1) - y1);
      this.context.lineTo(x2, (this.SCREEN_HEIGHT - 1) - y2);
      this.context.strokeStyle = this.fgndColor;
      this.context.stroke();
    }
  }

  showCursor() {
    let cursorY = ((this.SCREEN_HEIGHT - 1) - this.y) - (this.fontHeight - 1);
    this.cursorCharBlock = this.context.getImageData(this.x, cursorY, this.fontWidth, this.fontHeight);
    this.context.fillStyle = this.fgndColor;
    this.context.fillRect(this.x, cursorY, this.fontWidth, this.fontHeight);
  }

  hideCursor() {
    let cursorY = ((this.SCREEN_HEIGHT - 1) - this.y) - (this.fontHeight - 1);
    if (this.cursorCharBlock) {
      this.context.putImageData(this.cursorCharBlock, this.x, cursorY);
      delete this.cursorCharBlock;
    }
  }

  showCrosshairs() {
    this.canvas.style.cursor = "crosshair";
  }

  hideCrosshairs() {
    this.canvas.style.cursor = "default";
  }

  renderText(data) {

    if (typeof data === "string") {
      let ab = new Uint8Array(data.length);
      for (let i = 0; i < data.length; i++) {
        ab[i] = data.charCodeAt(i) & 0xff;
      }
      data = ab;
    }

    for (let i = 0; i < data.byteLength; i++) {
      let b = data[i] & 0x7f;
      switch (this.state) {
      case this.ST_NORMAL:
        switch (b) {
        case 0x08: // <BS>
          this.hideCursor();
          this.x -= this.fontWidth;
          if (this.x < this.marginOffset) {
            this.y += this.fontHeight;
            if (this.y + this.fontHeight > this.SCREEN_HEIGHT) {
              this.marginOffset = (this.marginOffset > 0) ? 0 : Math.round(this.SCREEN_WIDTH / 2);
              this.y = 0;
            }
            this.x = this.SCREEN_WIDTH - this.fontWidth;
          }
          this.showCursor();
          break;
        case 0x09: // <HT>
          this.hideCursor();
          this.x += this.fontWidth;
          if (this.x >= this.SCREEN_WIDTH) {
            this.x = this.marginOffset;
            this.y -= this.fontHeight;
            if (this.y < 0) {
              if (this.marginOffset > 0) {
                this.x -= this.marginOffset;
                this.marginOffset = 0;
              }
              else {
                this.marginOffset = Math.round(this.SCREEN_WIDTH / 2);
                if (this.x < this.marginOffset) this.x += this.marginOffset;
              }
              this.y = this.SCREEN_HEIGHT - this.fontHeight;
            }
          }
          this.showCursor();
          break;
        case 0x0a: // <LF>
          this.hideCursor();
          this.y -= this.fontHeight;
          if (this.y < 0) {
            if (this.marginOffset > 0) {
              this.x -= this.marginOffset;
              this.marginOffset = 0;
            }
            else {
              this.marginOffset = Math.round(this.SCREEN_WIDTH / 2);
              if (this.x < this.marginOffset) this.x += this.marginOffset;
            }
            this.y = this.SCREEN_HEIGHT - this.fontHeight;
          }
          this.isBypass = false;
          this.showCursor();
          break;
        case 0x0b: // <VT>
          this.hideCursor();
          this.y += this.fontHeight;
          if (this.y + this.fontHeight > this.SCREEN_HEIGHT) {
            if (this.marginOffset > 0) {
              this.x -= this.marginOffset;
              this.marginOffset = 0;
            }
            else {
              this.marginOffset = Math.round(this.SCREEN_WIDTH / 2);
              if (this.x < this.marginOffset) this.x += this.marginOffset;
            }
            this.y = 0;
          }
          this.isBypass = false;
          this.showCursor();
          break;
        case 0x0d: // <CR>
          if (this.isGraphMode) {
            if (this.lastX) {
              this.x = this.lastX;
              this.y = this.lastY;
            }
            this.showCursor();
            this.isGraphMode = false;
          }
          else if (this.isGINMode) {
            this.hideCrosshairs();
            this.isGINMode = false;
          }
          else {
            this.hideCursor();
            this.x = this.marginOffset;
            this.showCursor();
          }
          this.isBypass = false;
          break;
        case 0x1b: // <ESC>
          this.state = this.ST_ESCAPE;
          break;
        case 0x1d: // <GS>
          this.hideCursor();
          this.isGraphMode = true;
          delete this.lastX;
          delete this.lastY;
          this.coords = [];
          break;
        case 0x1f: // <US>
          if (this.lastX) {
            this.x = this.lastX;
            this.y = this.lastY;
          }
          this.showCursor();
          this.isGraphMode = false;
          break;
        case 0x00: // <NUL>
        case 0x01: // <SOH>
        case 0x02: // <STX>
        case 0x03: // <ETX>
        case 0x04: // <EOT>
        case 0x05: // <ENQ>
        case 0x06: // <ACK>
        case 0x07: // <BEL>
        case 0x0c: // <FF>
        case 0x0e: // <SO>
        case 0x0f: // <SI>
        case 0x10: // <DLE>
        case 0x11: // <DC1>
        case 0x12: // <DC2>
        case 0x13: // <DC3>
        case 0x14: // <DC4>
        case 0x15: // <NAK>
        case 0x16: // <SYN>
        case 0x17: // <ETB>
        case 0x18: // <CAN>
        case 0x19: // <EM>
        case 0x1a: // <SUB>
        case 0x1c: // <FS>
        case 0x1e: // <RS>
          this.isIgnoreCR = false;
          break;
        default:
          if (this.isGraphMode) {
            this.coords.push(b);
            if (b >= 0x40 && b <= 0x5f) {
              for (let i = 0; i < this.coords.length; i++) {
                b = this.coords[i];
                if (b >= 0x20 && b <= 0x3f) { // hiX or hiY
                  if (i === 0) {
                    this.hiY = b & 0x1f;
                  }
                  else {
                    this.hiX = b & 0x1f;
                  }
                }
                else if (b >= 0x40 && b <= 0x5f) {
                  this.loX = b & 0x1f;
                }
                else {
                  this.loY = b & 0x1f;
                }
              }
              this.coords = [];
              let nextX = (this.hiX << 5) | this.loX;
              let nextY = (this.hiY << 5) | this.loY;
              if (this.lastX) {
                this.drawVector(this.lastX, this.lastY, nextX, nextY);
              }
              this.lastX = nextX;
              this.lastY = nextY;
            }
          }
          else {
            this.hideCursor();
            this.drawChar(b);
            this.x += this.fontWidth;
            if (this.x >= this.SCREEN_WIDTH) {
              this.y -= this.fontHeight;
              if (this.y < 0) {
                this.marginOffset = (this.marginOffset > 0) ? 0 : Math.round(this.SCREEN_WIDTH / 2);
                this.y = this.SCREEN_HEIGHT - this.fontHeight;
              }
              this.x = this.marginOffset;
            }
            this.showCursor();
          }
          break;
        }
        break;

      case this.ST_ESCAPE: // Process byte following <ESC>
        switch (b) {
        case 0x05: // <ENQ>
          let response = "";
          if (this.isGINMode) {
            this.hideCrosshairs();
            this.isGINMode = false;
            response += String.fromCharCode((this.crosshairX >> 5)   | 0x40)
                     +  String.fromCharCode((this.crosshairX & 0x1f) | 0x40)
                     +  String.fromCharCode((this.crosshairY >> 5)   | 0x40)
                     +  String.fromCharCode((this.crosshairY & 0x1f) | 0x40);
          }
          else {
            let statusByte = 0x30; // hardcopy unit not available
            if (this.marginOffset > 0) statusByte |= 0x02;
            if (this.isGraphMode) {
              statusByte |= 0x09; // graph mode and no auxiliary device
            }
            else {
              statusByte |= 0x05; // alpha mode and no auxiliary device
            }
            response += String.fromCharCode(statusByte)
                     +  String.fromCharCode((this.x >> 5)   | 0x40)
                     +  String.fromCharCode((this.x & 0x1f) | 0x40)
                     +  String.fromCharCode((this.y >> 5)   | 0x40)
                     +  String.fromCharCode((this.y & 0x1f) | 0x40);
          }
          this.keyboardInputHandler(response);
          break;
        case 0x0c: // <FF>
          this.x            = 0;
          this.y            = this.SCREEN_HEIGHT - this.fontHeight;
          this.marginOffset = 0;
          this.isGraphMode  = false;
          if (this.isGINMode) {
            this.hideCrosshairs();
            this.isGINMode = false;
          }
          this.isBypass     = false;
          this.clearScreen();
          break;
        case 0x0a: // <LF>
        case 0x0d: // <CR> ignore further <CR>s next non-operative control
          this.isIgnoreCR = true;
          break;
        case 0x0e: // <SO>
          this.fontType = "apl";
          this.setFont(this.fontType);
          break;
        case 0x0f: // <SI>
          this.fontType = "normal";
          this.setFont(this.fontType);
          break;
        case 0x17: // <ETB>
          this.isBypass = false;
          break;
        case 0x18: // <CAN>
          this.isBypass = true;
          break;
        case 0x1a: // <SUB>
          this.isGINMode = true;
          this.isGraphMode = false;
          this.isBypass = true;
          this.showCrosshairs();
          break;
        case 0x38: // '8' select 35x74
          this.screenDims = "35x74";
          this.setFont(this.fontType);
          break;
        case 0x39: // '9' select 38x81
          this.screenDims = "38x81";
          this.setFont(this.fontType);
          break;
        case 0x3a: // ':' select 58x121
          this.screenDims = "58x121";
          this.setFont(this.fontType);
          break;
        case 0x3b: // ';' select 64x133
          this.screenDims = "64x133";
          this.setFont(this.fontType);
          break;
        }
        this.state = this.ST_NORMAL;
        break;
      }
    }
  }
}
