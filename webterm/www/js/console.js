/*--------------------------------------------------------------------------
**
**  Copyright (c) 2023, Kevin Jordan
**
**  console.js
**    This class implements a Cyber console emulator.
**
**--------------------------------------------------------------------------
*/

class CyberConsole {

  constructor() {

    //
    // Font sizes
    //
    this.DOT_FONT        = 0;
    this.SMALL_FONT      = 1;
    this.MEDIUM_FONT     = 2;
    this.LARGE_FONT      = 3;

    //
    // Console screen offsets
    //
    this.SCREEN_GAP      = 40;
    this.SCREEN_MARGIN   = 20;

    //
    // Console stream commands
    //
    this.CMD_SET_X_LOW     = 0x80;
    this.CMD_SET_Y_LOW     = 0x81;
    this.CMD_SET_X_HIGH    = 0x82;
    this.CMD_SET_Y_HIGH    = 0x83;
    this.CMD_SET_SCREEN    = 0x84;
    this.CMD_SET_FONT_TYPE = 0x85;

    //
    // Console states
    //
    this.ST_TEXT           = 0;
    this.ST_COLLECT_SCREEN = 1;
    this.ST_COLLECT_FONT   = 2;
    this.ST_COLLECT_X      = 3;
    this.ST_COLLECT_Y      = 4;

    //
    // Console emulation properties
    //
    this.bgndColor         = "black";
    this.currentFont       = this.SMALL_FONT;
    this.fgndColor         = "lightgreen";
    this.fontFamily        = "Lucida Typewriter";
    this.fontHeights       = [2, 10, 20, 40];
    this.offscreenCanvas   = null;
    this.offscreenContext  = null;
    this.onscreenCanvas    = null;
    this.state             = this.ST_TEXT;
    this.x                 = 0;
    this.y                 = 0;

    //
    // Calculate font information
    //
    this.fonts      = [""];
    this.fontWidths = [2];
    let testLine = "MMMMMMMMMMMMMMMM" // 64 M's
                 + "MMMMMMMMMMMMMMMM"
                 + "MMMMMMMMMMMMMMMM"
                 + "MMMMMMMMMMMMMMMM";
    let cnv = document.createElement("canvas");
    cnv.width = testLine.length * 16;
    cnv.height = this.fontHeights[this.SMALL_FONT];
    let ctx = cnv.getContext("2d");
    let font = `${this.fontHeights[this.SMALL_FONT]}px ${this.fontFamily}`;
    this.fonts.push(font);
    ctx.font = font;
    let fontWidth = Math.round(ctx.measureText(testLine).width / testLine.length);
    this.xRatio = fontWidth / 8;
    this.yRatio = this.fontHeights[this.SMALL_FONT] / 8;
    this.fontWidths.push(fontWidth);
    for (const fontSize of [this.MEDIUM_FONT, this.LARGE_FONT]) {
      this.fonts.push(`${this.fontHeights[fontSize]}px ${this.fontFamily}`);
      fontWidth *= 2;
      this.fontWidths.push(fontWidth);
    }
    this.screenWidth  = this.fontWidths[this.SMALL_FONT] * 64;
    this.screenHeight = this.fontHeights[this.SMALL_FONT] * 64;
    this.canvasWidth  = (this.screenWidth * 2) + this.SCREEN_GAP + (this.SCREEN_MARGIN * 2);
    this.canvasHeight = this.screenHeight + (this.SCREEN_MARGIN * 2);
  }

  setFont(font) {
    this.currentFont = font;
    this.offscreenContext.font = this.fonts[font];
  }

  setUplineDataSender(callback) {
    this.uplineDataSender = callback;
  }

  getCanvas() {
    return this.onscreenCanvas;
  }

  getWidth() {
    return this.onscreenCanvas.width;
  }

  processKeyboardEvent(keyStr, shiftKey, ctrlKey, altKey) {
    let sendStr = "";
    //
    // Handle non-special keys
    //
    if (keyStr.length < 2) { // non-special key
      if (ctrlKey === false && altKey === false) {
        sendStr = keyStr;
      }
    }
    //
    // Handle special keys
    //
    else {

      switch (keyStr) {
      case "Backspace":
      case "Delete":
        sendStr = "\b";
        break;
      case "Enter":
        sendStr = "\r";
        break;
      default: // ignore the key
        break;
      }
    }
    if (sendStr.length > 0 && this.uplineDataSender) {
      this.uplineDataSender(sendStr);
    }
  }

  createScreen() {
    this.onscreenCanvas = document.createElement("canvas");
    this.onscreenContext = this.onscreenCanvas.getContext("bitmaprenderer");
    this.onscreenCanvas.width = this.canvasWidth;
    this.onscreenCanvas.height = this.canvasHeight;
    this.onscreenCanvas.style.cursor = "default";
    this.onscreenCanvas.style.border = "1px solid black";
    this.onscreenCanvas.setAttribute("tabindex", 0);
    this.onscreenCanvas.setAttribute("contenteditable", "true");
    this.offscreenCanvas = new OffscreenCanvas(this.canvasWidth, this.canvasHeight);
    this.offscreenContext = this.offscreenCanvas.getContext("2d");
    this.offscreenContext.textBaseline = "top";
    this.reset();
    this.clearScreen();

    const me = this;

    this.onscreenCanvas.addEventListener("click", () => {
      me.onscreenCanvas.focus();
    });

    this.onscreenCanvas.addEventListener("mouseover", () => {
      me.onscreenCanvas.focus();
    });

    this.onscreenCanvas.addEventListener("keydown", evt => {
      evt.preventDefault();
      me.processKeyboardEvent(evt.key, evt.shiftKey, evt.ctrlKey, evt.altKey);
    });
  }

  clearScreen() {
    this.offscreenContext.fillStyle = this.bgndColor;
    this.offscreenContext.clearRect(0, 0, this.canvasWidth, this.canvasHeight);
    this.offscreenContext.fillRect(0, 0, this.canvasWidth, this.canvasHeight);
    this.offscreenContext.fillStyle = this.fgndColor;
  }

  reset() {
    this.setFont(this.SMALL_FONT);
    this.offscreenContext.fillStyle = this.fgndColor;
    this.state        = this.ST_TEXT;
    this.x            = 0;
    this.xOffset      = this.SCREEN_MARGIN;
    this.y            = 0;
  }

  start() {
    const me = this;
    let iteration = 0;
    this.timer = setInterval(() => {
      const bitmap = me.offscreenCanvas.transferToImageBitmap();
      me.onscreenContext.transferFromImageBitmap(bitmap);
      me.clearScreen();
    }, 1000/10);
  }

  stop() {
    clearInterval(this.timer);
  }

  displayNotification(font, x, y, s) {
    this.stop();
    this.currentFont = font;
    this.x           = x;
    this.xOffset     = 0;
    this.y           = y;
    this.state       = this.ST_TEXT;
    this.clearScreen();
    for (const line of s.split("\n")) {
      this.renderText(line);
      this.x  = x;
      this.y += this.fontHeights[this.currentFont];
    }
    const bitmap = this.offscreenCanvas.transferToImageBitmap();
    this.onscreenContext.transferFromImageBitmap(bitmap);
  }

  drawChar(b) {
    this.offscreenContext.fillText(String.fromCharCode(b), this.x + this.xOffset, this.y);
  }

  drawPoint() {
    let x = this.x + this.xOffset;
    this.offscreenContext.clearRect(x, this.y, 2, 2);
    this.offscreenContext.fillRect(x, this.y, 2, 2);
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
      let b = data[i];
      switch (this.state) {
      case this.ST_TEXT:
        if (b < 0x80) {
          if (this.currentFont === this.DOT_FONT) {
            this.drawPoint();
          }
          else {
            if (b < 0x20) b = 0x20;
            this.drawChar(b);
            this.x += this.fontWidths[this.currentFont];
          }
        }
        else {
          switch (b) {
          case this.CMD_SET_X_LOW:
            this.x = 0;
            this.state = this.ST_COLLECT_X;
            break;
          case this.CMD_SET_Y_LOW:
            this.y = 0;
            this.state = this.ST_COLLECT_Y;
            break;
          case this.CMD_SET_X_HIGH:
            this.x = 256;
            this.state = this.ST_COLLECT_X;
            break;
          case this.CMD_SET_Y_HIGH:
            this.y = 256;
            this.state = this.ST_COLLECT_Y;
            break;
          case this.CMD_SET_SCREEN:
            this.state = this.ST_COLLECT_SCREEN;
            break;
          case this.CMD_SET_FONT_TYPE:
            this.state = this.ST_COLLECT_FONT;
            break;
          default:
            // ignore the byte
            break;
          }
        }
        break;

      case this.ST_COLLECT_X:
        this.x = Math.round((this.x + b) * this.xRatio);
        this.state = this.ST_TEXT;
        break;

      case this.ST_COLLECT_Y:
        this.y = Math.round((0o777 - (this.y + b)) * this.yRatio);
        this.state = this.ST_TEXT;
        break;

      case this.ST_COLLECT_SCREEN:
        this.xOffset = (b === 1) ? this.screenWidth + this.SCREEN_GAP + this.SCREEN_MARGIN : this.SCREEN_MARGIN;
        this.state = this.ST_TEXT;
        break;

      case this.ST_COLLECT_FONT:
        this.currentFont = b;
        this.state = this.ST_TEXT;
        break;

      default:
        // ignore byte
        break;
      }
    }
  }
}
//
// The following lines enable this file to be used as a Node.js module.
//
if (typeof module === "undefined") module = {};
module.exports = CyberConsole;
