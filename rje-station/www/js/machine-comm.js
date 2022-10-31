/*
 *  This class manages data communication between a machine and an
 *  RJE station emulator.
 */
class Machine {

  constructor(id) {
    this.id = id;
    this.debug = false;
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

  createConnection(callback) {
    const me = this;
    this.url = `/machines?machine=${this.id}`;
    let req  = new XMLHttpRequest();
    req.onload = evt => {
      if (req.status !== 200) {
        if (typeof callback === "function") {
          callback(`${req.status} ${req.statusText}`);
        }
        return;
      }
      let id = req.responseText;
      let protocol = (location.protocol === "https:") ? "wss://" : "ws://";
      let socketURL = protocol + location.hostname
        + ((location.port) ? (":" + location.port) : "") + "/connections/" + id;
      me.ws = new WebSocket(socketURL);
      me.ws.binaryType = "arraybuffer";
      me.sender = data => {
        me.ws.send(data);
      }
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
      me.ws.onclose = () => {
        if (typeof me.disconnectListener === "function") {
          me.disconnectListener();
        }
      };
      if (typeof callback === "function") callback();
    };
    req.open("GET", this.url);
    req.send();
    return this.url;
  }

  isConnected() {
    return typeof this.ws === "object"
        && typeof this.ws.readyState !== "undefined"
        && (this.ws.readyState === 0 /*CONNECTING*/ || this.ws.readyState === 1 /*OPEN*/);
  }

  closeConnection() {
    if (typeof this.ws !== "undefined") {
      this.ws.onclose = () => {}; // disable onclose handler
      this.ws.close();
      delete this.ws;
    }
  }

  send(data) {
    this.sender(data);
  }
}
