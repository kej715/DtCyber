/*
 *  This class manages data communication between a machine and a
 *  terminal emulator.
 */
class Machine {

  constructor(id) {
    this.id = id;
    this.debug = false;
    this.script = [];
    this.scriptIndex = 0;
    this.keystrokeDelay = 30;
    this.isTerminalWait = false;
    this.buttons = {};
    this.containerStack = [];
  }

  getId() {
    return this.id;
  }

  setId(id) {
    this.id = id;
  }

  setReceivedDataHandler(receivedDataHandler) {
    if (this.savedState) {
      this.savedState.receivedDataHandler = receivedDataHandler;
    }
    else if (this.embeddedMachine) {
      this.embeddedMachine.setTerminalOutputHandler(receivedDataHandler);
    }
    else {
      this.receivedDataHandler = receivedDataHandler;
    }
  }

  setConnectListener(callback) {
    if (this.savedState) {
      this.savedState.connectListener = callback;
    }
    else {
      this.connectListener = callback;
    }
  }

  setDisconnectListener(callback) {
    if (this.savedState) {
      this.savedState.disconnectListener = callback;
    }
    else {
      this.disconnectListener = callback;
    }
  }

  setSender(sender) {
    this.sender = sender;
  }

  setTerminal(terminal) {
    this.terminal = terminal;
  }

  getTerminal() {
    return this.terminal;
  }

  getWebSocket() {
    return this.ws;
  }

  registerButton(name, btn) {
    this.buttons[name] = btn;
  }

  createConnection() {
    this.url = `/machines?machine=${this.id}`;
    let me = this;
    const req = new XMLHttpRequest();
    req.onload = evt => {
      let id = req.responseText;
      let protocol = (location.protocol === "https:") ? "wss://" : "ws://";
      let socketURL = protocol + location.hostname
        + ((location.port) ? (":" + location.port) : "") + "/connections/" + id;
      me.ws = new WebSocket(socketURL);
      me.ws.binaryType = "arraybuffer";
      me.sender = data => {
        me.ws.send(data);
      };
      me.ws.onmessage = evt => {
        let ary;
        if (evt.data instanceof ArrayBuffer) {
          ary = new Uint8Array(evt.data);
        }
        else if (typeof evt.data === "string") {
          ary = new Uint8Array(evt.data.length);
          for (let i = 0; i < evt.data.length; i++) {
            ary[i] = evt.data.charCodeAt(i) & 0xff;
          }
        }
        else {
          ary = evt.data;
        }
        if (me.terminal && me.terminal.isKSR) {
          ary = ary.map(b => b & 0x7f);
        }
        if (me.debug) {
          let s = "";
          for (let b of ary) {
            if (b >= 0x20 && b < 0x7f)
              s += String.fromCharCode(b);
            else if (b > 0x0f)
              s += `<${b.toString(16)}>`;
            else
              s += `<0${b.toString(16)}>`;
          }
          console.log(`${me.id}: ${s}`);
        }
        if (typeof me.receivedDataHandler === "function") {
          me.receivedDataHandler(ary);
        }
        else if (me.debug) {
          console.log(`Received data handler is ${typeof me.receivedDataHandler}`);
        }
      };
      me.ws.onopen = evt => {
        if (typeof me.connectListener === "function") {
          me.connectListener();
        }
      };
      me.ws.onclose = () => {
        delete me.ws;
        if (typeof me.disconnectListener === "function") {
          me.disconnectListener();
        }
      };
    };
    req.open("GET", this.url);
    req.send();
    return this.url;
  }

  isConnected() {
    return this.ws ? true : false;
  }

  closeConnection() {
    if (this.ws) {
      this.ws.onclose = function() {}; // disable onclose handler
      this.ws.close();
      delete this.ws;
    }
  }

  setEmbeddedMachine(machine) {
    this.embeddedMachine = machine;
    this.sender = data => {
      for (let i = 0; i < data.length; i++) {
        machine.receiveTerminalInput(data.charCodeAt(i));
      }
    }
    machine.setTerminalOutputHandler(this.receivedDataHandler);
  }

  send(data) {
    if (this.terminal && this.terminal.isKSR) {
      let ary = new Uint8Array(data.length);
      if (typeof data === "string") {
        for (let i = 0; i < data.length; i++) {
          let c = data.charCodeAt(i);
          if (c >= 0x61 && c <= 0x7a) {
            ary[i] = 0x80 | (c - 0x20);
          }
          else {
            ary[i] = 0x80 | c;
          }
        }
      }
      else {
        for (let i = 0; i < data.length; i++) {
          let c = data[i] & 0xff;
          if (c >= 0x61 && c <= 0x7a) {
            ary[i] = 0x80 | (c - 0x20);
          }
          else {
            ary[i] = 0x80 | c;
          }
        }
      }
      data = ary;
    }
    this.sender(data);
  }

  saveState(callback) {
    if (!this.savedState) {
      if (this.embeddedMachine) {
        this.savedState = {
          terminalOutputHandler: this.embeddedMachine.getTerminalOutputHandler(),
          callback: callback
        };
      }
      else {
        this.savedState = {
          receivedDataHandler: this.receivedDataHandler,
          connectListener: this.connectListener,
          disconnectListener: this.disconnectListener,
          callback: callback
        };
      }
    }
    else if (this.debug) {
      console.log("runScript: saveState called with state already saved");
    }
  }

  restoreState() {
    if (this.savedState) {
      if (this.embeddedMachine) {
        this.embeddedMachine.setTerminalOutputHandler(this.savedState.terminalOutputHandler);
      }
      else {
        this.receivedDataHandler = this.savedState.receivedDataHandler;
        this.connectListener = this.savedState.connectListener;
        this.disconnectListener = this.savedState.disconnectListener;
      }
      if (this.savedState.callback) {
        let callback = this.savedState.callback;
        delete this.savedState;
        callback();
      }
      else {
        delete this.savedState;
      }
    }
  }

  notifyTerminalStatus(terminalStatus) {
    //
    // This method is called only by TN3270
    //
    if (this.scriptIndex < this.script.length) {
      if (this.isTerminalWait !== terminalStatus.isTerminalWait) {
        this.isTerminalWait = terminalStatus.isTerminalWait;
        this.executeScript();
      }
    }
  }

  executeScript() {
    //
    //  A script is a list of instructions. Each instruction
    //  begins with a letter indicating an action, and the remainder
    //  of the instruction provides text or arguments for the action.
    //  Recognized actions are:
    //    A : Assign value to variable
    //    B : Click a specified terminal emulator button
    //    D : Delay by a specified number of milliseconds
    //    d : Set delay in milliseconds between keystrokes
    //    F : Execute a function with optional arguments, and send
    //        its result as a string
    //    G : Goto a specified line
    //    I : Send a sequence of lines, each ended by <CR>,
    //        prompted by the same pattern, and followed by
    //        the same delay. Subsequent instructions represent
    //        the lines to be sent, until a specified boundary
    //        line is encountered. For non-3270, syntax is:
    //           I<delay> <pattern> <boundary>
    //        and for 3270, syntax is:
    //           I<delay> <boundary>
    //    K : Send a key name, optionally prefaced by "Shift", "Ctrl", and/or "Alt"
    //        e.g.: KShift F1
    //    L : Send a single line
    //    S : Send a string
    //    T : Test a string against a regular expression and branch to a specified
    //        line if the string matches the expression
    //    U : Wait for keyboard unlock (Tn3270)
    //    W : Wait for text matching specified pattern
    //
    let me = this;
    const interpolate = text => {
      let result = "";
      const matchedSubstringsRE = /\$\{[0-9]+\}/g;
      const scriptVariableRE = /\$\{[A-Za-z_][A-Za-z_0-9]+\}/g;
      let mi = text.search(matchedSubstringsRE);
      let si = text.search(scriptVariableRE);
      while (mi >= 0 || si >= 0) {
        let i = mi >= 0 ? mi : si;
        let bi = text.indexOf("}", i + 2);
        let val = text.substring(i + 2, bi);
        result += text.substring(0, i);
        if (mi >= 0) {
          let n = parseInt(val);
          if (me.matchedSubstrings && n < me.matchedSubstrings.length) {
            result += me.matchedSubstrings[n];
          }
        }
        else if (me.scriptVariables[val]) {
          result += me.scriptVariables[val];
        }
        text = text.substring(bi + 1);
        mi = text.search(matchedSubstringsRE);
        si = text.search(scriptVariableRE);
      }
      result += text;
      return result;
    };
    const unescape = text => {
      //
      // unescape - function to translate \-escapes into
      //            corresponding control characters
      //
      let result = "";
      while (text.length > 0) {
        let si = text.indexOf("\\");
        if (si >= 0) {
          result += text.substr(0, si);
          if (si + 1 < text.length) {
            let c = text.charAt(si + 1);
            text = text.substr(si + 2);
            switch (c) {
            case "b":
              result += "\b";
              break;
            case "f":
              result += "\f";
              break;
            case "n":
              result += "\n";
              break;
            case "r":
              result += "\r";
              break;
            case "t":
              result += "\t";
              break;
            case "x":
              if (text.length > 1) {
                result += String.fromCharCode(parseInt(text.substr(0, 2), 16));
                text = text.substr(2);
              }
              break;
            default:
              result += c;
              break;
            }
          }
          else {
            break;
          }
        }
        else {
          result += text;
          break;
        }
      }
      return result;
    };
    //
    // send - send a string to the machine by simulating keyboard events
    //
    const sendText = (text, callback) => {
      if (text.length > 0) {
        let c = text.charAt(0);
        let code = text.charCodeAt(0);
        if (code < 0x20) {
          if (code > 0x00 && code <= 0x1a) {
            c = String.fromCharCode(code + 0x60);
          }
          else {
            c = String.fromCharCode(code + 0x40);
          }
          me.terminal.processKeyboardEvent(c, false, true, false);
        }
        else if (code >= 0x41 && code <= 0x5a) {
          me.terminal.processKeyboardEvent(c, true, false, false);
        }
        else {
          me.terminal.processKeyboardEvent(c, false, false, false);
        }
        let remainder = text.substr(1);
        setTimeout(() => {
          sendText(remainder, callback);
        }, me.keystrokeDelay);
      }
      else if (typeof callback === "function") {
        callback();
      }
    };
    //
    // logReceivedText - log text received from the machine.
    //                   Make non-displayable bytes readable.
    //
    const logReceivedText = text => {
      let s = "";
      for(let i = 0; i < text.length; i++) {
        let c = text.charCodeAt(i);
        if (c <= 0x20 || c > 0x7e) {
          s += `<${c.toString(16)}>`;
        }
        else {
          s += text.charAt(i);
        }
      }
      console.log(`runScript: receivedText: ${s}`);
    };
    //
    // Process script entries
    //
    let done = false;
    while (this.scriptIndex < this.script.length
           && typeof this.queuedText === "undefined"
           && typeof this.delayTimer === "undefined"
           && done === false) {
      let scriptEntry = this.script[this.scriptIndex];
      if (scriptEntry.length < 1) {
        this.scriptIndex += 1;
        continue;
      }
      let action = scriptEntry.charAt(0);
      let text = scriptEntry.substr(1);
      let args = text.split(" ");
      switch (action) {
      case "A": // Assign value to variable
        if (args.length === 2) {
          if (this.debug) console.log(`runScript: ${action}${text} <<${args[0]}=${interpolate(args[1])}>>`);
          this.scriptVariables[args[0]] = interpolate(args[1]);
        }
        else {
          console.log(`runScript: incorrect number of arguments in ${action}${text}`);
        }
        this.scriptIndex += 1;
        break;
      case "B": // Click button
        if (this.buttons[text]) {
          if (this.debug) console.log(`runScript: ${action}${text}`);
          this.buttons[text].click();
        }
        else {
          console.log(`runScript: no such button: ${text}`);
        }
        this.scriptIndex += 1;
        break;
      case "D": // Delay
        if (this.debug) console.log(`runScript: ${action}${text}`);
        let interval = parseInt(text);
        this.delayTimer = setTimeout(() => {
          if (this.debug) console.log(`runScript: ${action}${text} <<delay complete>>`);
          delete me.delayTimer;
          me.scriptIndex += 1;
          me.executeScript();
        }, interval);
        break;
      case "d": // Delay between keystrokes
        this.keystrokeDelay = parseInt(text);
        this.scriptIndex += 1;
        break;
      case "F": // Execute a function with arguments and send result as string
        text = interpolate(text);
        if (this.debug) console.log(`runScript: ${action}${text}`);
        this.queuedText = "";
        if (args.length > 0) {
          switch (args[0]) {
          //
          // date function
          //
          case "date":
            let date = new Date();
            let mon = date.getMonth() + 1;
            let day = date.getDate();
            let year = date.getFullYear();
            if (args.length > 1) {
              switch (args[1]) {
              case "MMDDYY":
                this.queuedText = `${mon >= 10 ? mon : "0" + mon}${day >= 10 ? day : "0" + day}${year - 2000}`;
                break;
              case "MM/DD/YYYY":
                this.queuedText = `${mon >= 10 ? mon : "0" + mon}/${day >= 10 ? day : "0" + day}/${year}`;
                break;
              default:
                console.log(`runScript: unrecognized date format "${args[1]}" in ${action}${text}`);
                break;
              }
            }
            else {
              this.queuedText = date.toLocaleString(); 
            }
            break;
          default:
            console.log(`runScript: unrecognized function "${args[0]}" in ${action}${text}`);
            break;
          }
        }
        sendText(this.queuedText, () => {
          if (this.debug) console.log(`runScript: ${action} <<send complete>>`);
          delete me.queuedText;
          me.scriptIndex += 1;
          me.executeScript();
        });
        break;
      case "G": // Goto specified line
        if (this.debug) console.log(`runScript: ${action}${text}`);
        if (this.scriptLabels[text]) {
          this.scriptIndex = this.scriptLabels[text];
        }
        else {
          console.log(`runScript: undefined label "${text}" in ${action}${text}`);
          this.scriptIndex += 1;
        }
        break;
      case "K": // Send key name with optional modifier(s)
        let shift = false;
        let ctrl  = false;
        let alt   = false;
        let keyNames = text.split(" ");
        while (keyNames.length > 1) {
          let modifier = keyNames.shift().toLowerCase();
          switch (modifier) {
          case "shift":
            shift = true;
            break;
          case "ctrl":
            ctrl = true;
            break;
          case "alt":
            alt = true;
            break;
          default:
            console.log(`runScript: unrecognized modifier "${modifier}" in ${action}${text}`);
            break;
          }
        }
        this.terminal.processKeyboardEvent(keyNames.shift(), shift, ctrl, alt);
        this.scriptIndex += 1;
        break;
      case "L": // Send line
      case "S": // Send string
        text = interpolate(unescape(text));
        if (this.debug) console.log(`runScript: ${action}${text}`);
        this.queuedText = text;
        sendText(text, () => {
          if (action === "L") this.terminal.processKeyboardEvent("Enter", false, false, false);
          if (this.debug) console.log(`runScript: ${action} <<send complete>>`);
          delete me.queuedText;
          me.scriptIndex += 1;
          me.executeScript();
        });
        break;
      case "T": // Test string for match against regular expression
        if (args.length === 3) {
          let str = interpolate(unescape(args[0]));
          let re = new RegExp(interpolate(unescape(args[1]), "g"));
          if (this.debug) console.log(`runScript: ${action}${str} ${interpolate(args[1])} ${args[2]} <<${re.test(str)}>>`);
          if (re.test(str)) {
            if (this.scriptLabels[args[2]]) {
              this.scriptIndex = this.scriptLabels[args[2]];
            }
            else {
              console.log(`runScript: undefined label "${args[2]}" in ${action}${text}`);
              this.scriptIndex += 1;
            }
          }
          else {
            this.scriptIndex += 1;
          }
        }
        else {
          console.log(`runScript: incorrect number of arguments in ${action}${text}`);
          this.scriptIndex += 1;
        }
        break;
      case "U": // Wait for 3270 keyboard unlock
        if (this.isTerminalWait === false) {
          if (this.debug) console.log(`runScript: ${action} <<unlocked>>`);
          this.scriptIndex += 1;
        }
        else {
          if (this.debug) console.log(`runScript: ${action} <<locked>>`);
          done = true;
        }
        break;
      case "W": // Wait for match
        if (this.receivedText.length > 0) {
          if (this.debug) {
            logReceivedText(this.receivedText);
            console.log(`runScript: ${action}${text}`);
          }
          let re = new RegExp(interpolate(unescape(text)).replaceAll("\\n", "$"), "gm");
          let lines = this.receivedText.split("\n");
          while (lines.length > 0) {
            let line = lines.shift();
            let matches = re.exec(line);
            if (matches !== null) {
              this.matchedSubstrings = matches.slice(1);
              if (this.debug) {
                for (let i = 0; i < this.matchedSubstrings.length; i++) {
                  console.log(`runScript:   [${i}] ${this.matchedSubstrings[i]}`);
                }
              }
              line = line.substring(re.lastIndex);
              this.scriptIndex += 1;
            }
            else if (lines.length > 0) {
              continue;
            }
            else {
              done = true;
            }
            if (line.length > 0) lines.unshift(line);
            this.receivedText = lines.join("\n");
            break;
          }
        }
        else {
          done = true;
        }
        break;
      default:
        console.log(`runScript: unrecognized script action: '${action}'`);
        this.scriptIndex = this.script.length;
        break;
      }
    }
    if (this.scriptIndex >= this.script.length) {
      if (this.debug) console.log("runScript: end of script");
      this.restoreState();
    }
  }

  runScript(s, callback) {
    let me = this;
    this.script = [];
    this.scriptIndex = 0;
    this.receivedText = "";
    this.saveState(callback);
    if (typeof this.terminal.isTn3270 === "undefined" || this.terminal.isTn3270 === false) {
      if (this.embeddedMachine) {
        this.embeddedMachine.setTerminalOutputHandler(data => {
          if (me.savedState && typeof me.savedState.terminalOutputHandler === "function") {
            me.savedState.terminalOutputHandler(data);
          }
          me.receivedText += String.fromCharCode.apply(null, data);
          me.executeScript();
        });
      }
      else {
        this.receivedDataHandler = data => {
          if (me.savedState && typeof me.savedState.receivedDataHandler === "function") {
            me.savedState.receivedDataHandler(data);
          }
          me.receivedText += String.fromCharCode.apply(null, data);
          me.executeScript();
        };
      }
    }
    this.disconnectListener = () => {
      if (me.savedState && typeof me.savedState.disconnectListener === "function") {
        me.savedState.disconnectListener();
      }
      if (me.debug) console.log("runScript: end of script");
      me.restoreState();
    };
    //
    // Create list of script actions. Build map of line labels, and expand "I" actions
    // into sequences of L/W/D actions (L/D/U for Tn3270).
    //
    this.scriptLabels = {};
    this.scriptVariables = {};
      let lines = s.replaceAll("\r\n", "\n").split("\n");
    let lineNo = 0;
    let labelRE = /^:[A-Za-z_0-9]+:/;
    while (lines.length > 0) {
      let line = lines.shift().trim();
      if (line.length < 1 || line.charAt(0) === "#") continue;
      if (labelRE.test(line)) {
        let ci = line.indexOf(":", 1);
        this.scriptLabels[line.substring(1, ci)] = lineNo;
        line = line.substring(ci + 1).trim();
        if (line.length < 1) continue;
      }
      if (line.charAt(0) === "I") {
        let args = line.substr(1).split(" ");
        if (this.terminal.isTn3270) {
          if (args.length === 2) {
            let delay = parseInt(args[0]);
            let boundary = args[1];
            while (lines.length > 0) {
              line = lines.shift();
              if (line === boundary) break;
              this.script.push(`L${line}`);
              if (delay > 0) {
                this.script.push(`D${delay}`);
                lineNo += 1;
              }
              this.script.push("U");
              lineNo += 2;
            }
          }
          else {
            console.log("runScript: invalid 'I' action syntax in script");
          }
        }
        else if (args.length === 3) {
          let delay = parseInt(args[0]);
          let promptPattern = args[1];
          let boundary = args[2];
          while (lines.length > 0) {
            line = lines.shift();
            if (line === boundary) break;
            this.script.push(`L${line}`);
            this.script.push(`W${promptPattern}`);
            lineNo += 2;
            if (delay > 0) {
              this.script.push(`D${delay}`);
              lineNo += 1;
            }
          }
        }
        else {
          console.log("runScript: invalid 'I' action syntax in script");
        }
      }
      else {
        this.script.push(line);
        lineNo += 1;
      }
    }
    //
    // Initiate script execution
    //
    this.executeScript();
  }
}
//
// The following lines enable this file to be used as a Node.js module.
//
if (typeof module === "undefined") module = {};
module.exports = Machine;
