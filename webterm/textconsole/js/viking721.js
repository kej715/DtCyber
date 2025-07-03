/*--------------------------------------------------------------------------
**
**  Copyright (c) 2025, Kevin Jordan
**
**  viking721.js
**    This module provides classes that emulate a CDC Viking 721 terminal
**    for usage as a NOS/VE console.
**
**--------------------------------------------------------------------------
*/
const terminalKit = require('terminal-kit');

class VikingConsole {

  constructor() {
    //
    // Character reception states
    //
    this.ST_BASE     = 0;
    this.ST_RS_1     = 1;
    this.ST_W_CURSOR = 2;
    this.ST_DEF_FN   = 3;
    this.ST_REPORT   = 4;
    this.ST_LOAD     = 5;
    this.ST_X_CHAR   = 6;
    this.ST_SET_SF   = 7;
    this.ST_RS_DC2   = 8;
    //
    // Terminal instance
    //
    this.terminal = terminalKit.terminal;
    //
    // Screen control
    //
    this.defaultAttributes = {
        bgColor:   'black',
        color:     'brightgreen',
        blink:     false,
        bold:      false,
        dim:       false,
        inverse:   false,
        underline: false
    };
    this.attributes       = this.defaultAttributes;
    this.height           = 30;
    this.screenBuffer     = null;
    this.scrollFieldLower = this.height - 1;
    this.scrollFieldUpper = 0;
    this.state            = this.ST_BASE;
    this.width            = 80;
    this.wrap             = true;
    this.x                = 0;
    this.y                = 0;
    //
    // Shutdown
    //
    this.shutdownListener = null;
  }

  clearScreen() {
    this.screenBuffer.fill({
      char: ' ',
      attr: {
        color: this.attributes.color,
        bgColor: this.attributes.bgColor
      }
    });

    this.x = 0;
    this.y = 0;
    this.screenBuffer.moveTo(this.x, this.y);
    this.screenBuffer.drawCursor();
    this.screenBuffer.draw({delta: false});
  }

  createScreen() {
    this.createScreenBuffer({
        width:this.width,
        height:this.height,
        dst:this.terminal,
        x: this.x,
        y: this.y,
        wrap: this.wrap
    });

    // Event listener for terminal resize
    this.terminal.on('resize', (width, height) => {
      this.screenBuffer.resize({width: width, height: height, x: 0, y: 0})
      this.clearScreen();
      this.updateScreen();
    });

    this.terminal.grabInput({});

    this.terminal.on('key', (name, matches, data) => {
      if (name.length === 1) {
        this.uplineDataSender(name);
      }
      else {
        switch (name) {
        case 'CTRL_M':
        case 'CTRL_J':
        case 'ENTER':
          this.uplineDataSender('\r');
          break;
        case 'BACKSPACE':
          this.uplineDataSender(String.fromCharCode(0x08));
          break;
        case 'TAB':
          this.uplineDataSender(String.fromCharCode(0x09));
          break;
        case 'DELETE':
          this.uplineDataSender(String.fromCharCode(0x7f));
          break;
        case 'CTRL_C':
          this.processExit();
          process.exit(0);
          break;
        default:
          break;
        }
      }
    });

    this.terminal.on('mouse', (name, data) => {
    });
  }

  createScreenBuffer() {
    this.terminal.fullscreen(true);
    this.terminal.hideCursor(false);

    this.screenBuffer = new terminalKit.ScreenBuffer({
      dst: this.terminal,
      width: this.width,
      height: this.height,
      wrap: false,
      noFill: false
    });
    this.clearScreen();
    this.updateScreen();
  }

  displayNotification(x, y, s) {
    this.clearScreen();
    this.x = x;
    this.y = y;
    for (const line of s.split("\n")) {
      this.renderText(line);
      this.x = x;
      this.y += 1;
    }
    this.updateScreen();
  }

  processBaseChar(b) {
    if (b >= 0x20 && b < 0x7f) {
      this.put(this.x, this.y, b);
      this.x += 1;
    }
    else {
      switch (b) {
      default:
        break;
      case 0x02: // STX - write cursor address
        this.params = [];
        this.state = this.ST_W_CURSOR;
        break;
      case 0x03: // ETX - enable blink
        break;
      case 0x04: // EOT - disable blink
        break;
      case 0x05: // ENQ - read cursor address
        break;
      case 0x06: // ACK - start underline
        break;
      case 0x07: // BEL
        this.terminal.bell();
        break;
      case 0x08: // BS  - cursor left
      case 0x1F: // US  - cursor left
        if (this.x > 0) this.x -= 1;
        break;
      case 0x09: // HT  - tab
        break;
      case 0x0a: // LF  - cursor down
      case 0x1a: // SUB - cursor down
        if (this.y < 29) {
          this.y += 1;
        }
        else {
          this.y = 0;
        }
        break;
      case 0x0b: // VT  - erase to end of line
        break;
      case 0x0c: // FF  - erase page
        break;
      case 0x0d: // CR  - carriage return
        this.x = 0;
        break;
      case 0x0e: // SO  - start blink
        break;
      case 0x0f: // SI  - stop blink
        break;
      case 0x11: // DC1 - x-on
        break;
      case 0x12: // DC2 - roll enable
        break;
      case 0x13: // DC3 - x-off
        break;
      case 0x15: // NAK - end underscore
        break;
      case 0x16: // SYN - roll disable
        break;
      case 0x17: // ETB - cursor up
        if (this.y > 0) this.y -= 1;
        break;
      case 0x18: // CAN - skip
        break;
      case 0x19: // EM  - home
        this.x = 0;
        this.y = 0;
        break;
      case 0x1c: // FS  - start dim
        break;
      case 0x1d: // GS  - end dim
        break;
      case 0x1e: // RS  - enter RS1 state
        this.state = this.ST_RS_1;
        break;
      }
    }
  }

  processDC2(b) {
    switch (b) {
    default:
      break;
    case 0x42: // enter large CYBER mode
      break;
    case 0x48: // set 80 character line
      this.width = 80;
      this.screenBuffer.resize({width: this.width, height: this.height, x: 0, y: 0})
      this.clearScreen();
      this.updateScreen();
      break;
    case 0x5e: // select 30 lines
      this.height = 30;
      this.screenBuffer.resize({width: this.width, height: this.height, x: 0, y: 0})
      this.clearScreen();
      this.updateScreen();
      break;
    }
    this.state = this.ST_BASE;
  }

  processDefFn(b) {
  }

  processExtChar(b) {
  }

  processLoadRAM(b) {
  }

  processReport(b) {
  }

  processRS1(b) {
    switch (b) {
    default:
      break;
    case 0x12: // RS DC2
      this.state = this.ST_RS_DC2;
      return;
    case 0x2a: // clear to end of line
      for (let x = this.x; x < this.width; x++) {
        this.put(x, this.y, 0x20);
      }
      break;
    case 0x33: // disable host loaded code
      break;
    case 0x44: // start inverse
      this.attributes.inverse = true;
      break;
    case 0x45: // end inverse
      this.attributes.inverse = false;
      break;
    case 0x55: // field scroll up
      this.screenBuffer.vScroll(-1, this.attributes, this.scrollFieldUpper, this.scrollFieldLower);
      break;
    case 0x57: // set scroll field
      this.params = [];
      this.state = this.ST_SET_SF;
      return;
    }
    this.state = this.ST_BASE;
  }

  processSetScrollField(b) {
    this.params.push(b);
    if (this.params.length > 1) {
      this.scrollFieldUpper = this.params[0] - 0x20;
      this.scrollFieldLower = this.params[1] - 0x20;
      this.state = this.ST_BASE;
    }
  }

  processWriteCursor(b) {
    this.params.push(b);
    if (this.params.length > 1) {
      this.x = this.params[0] - 0x20;
      this.y = this.params[1] - 0x20;
      this.state = this.ST_BASE;
    }
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
      case this.ST_BASE:     this.processBaseChar(b); break;
      case this.ST_RS_1:     this.processRS1(b); break;
      case this.ST_W_CURSOR: this.processWriteCursor(b); break;
      case this.ST_DEF_FN:   this.processDefFn(b); break;
      case this.ST_REPORT:   this.processReport(b); break;
      case this.ST_LOAD:     this.processLoadRAM(b); break;
      case this.ST_X_CHAR:   this.processExtChar(b); break;
      case this.ST_SET_SF:   this.processSetScrollField(b); break;
      case this.ST_RS_DC2:   this.processDC2(b); break;
      }
    }
    this.updateScreen();
  }

  setShutdownListener(callback) {
    this.shutdownListener = callback;
  }

  setUplineDataSender(callback) {
    this.uplineDataSender = callback;
  }

  shutdown() {
    this.terminal.fullscreen(false);
    this.terminal.hideCursor(false);
  }

  processExit() {
    // Ensures console cleaned up
    this.terminal.processExit(0);
  }

  put(x, y, b) {
    this.screenBuffer.put({
      x: x,
      y: y,
      attr: this.attributes
    }, String.fromCharCode(b));
  }

  updateScreen() {
    this.screenBuffer.moveTo(this.x, this.y);
    this.screenBuffer.drawCursor();
    this.screenBuffer.draw({delta: true});
  }
}

module.exports = VikingConsole;
