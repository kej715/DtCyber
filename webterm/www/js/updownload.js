class Downloader {

  constructor(machine, localFileName, hostFileName, isText, startScript, endScript, receiver) {
    this.machine       = machine;
    this.localFileName = localFileName;
    this.hostFileName  = hostFileName;
    this.isText        = isText;
    this.startScript   = startScript;
    this.endScript     = endScript;
    this.receiver      = receiver;
  }

  start(successCallback, errorCallback, progressCallback) {
    const me = this;
    let script = this.startScript;
    while (true) {
      let idx = script.indexOf("${filename}");
      if (idx >= 0) {
        let prefix = script.substring(0, idx);
        let suffix = script.substring(idx + 11);
        script = prefix + this.hostFileName + suffix;
      }
      else {
        break;
      }
    }
    this.machine.runScript(script, () => {
      me.receiver.receive(me.isText, me.localFileName,
        data => {
          if (me.endScript !== null && me.endScript !== "") {
            me.machine.runScript(me.endScript, () => {
              successCallback(data);
            });
          }
          else {
            successCallback(data);
          }
        },
        msg => {
          if (me.endScript !== null && me.endScript !== "") {
            me.machine.runScript(me.endScript, () => {
              errorCallback(msg);
            });
          }
          else {
            errorCallback(msg);
          }
        },
        progressCallback);
    });
  }
}

class Uploader {

  constructor(machine, localFile, hostFileName, isText, startScript, endScript, transmitter) {
    this.machine      = machine;
    this.localFile    = localFile;
    this.hostFileName = hostFileName;
    this.isText       = isText;
    this.startScript  = startScript;
    this.endScript    = endScript;
    this.transmitter  = transmitter;
  }

  start(successCallback, errorCallback, progressCallback) {
    const me = this;
    const reader = new FileReader();
    reader.onload = evt => {
      const data = new Uint8Array(evt.target.result);
      me.transmitter.transmit(data, me.isText, me.hostFileName,
        () => {
          if (me.endScript !== null && me.endScript !== "") {
            me.machine.runScript(me.endScript, () => {
              successCallback();
            });
          }
          else {
            successCallback();
          }
        },
        msg => {
          if (me.endScript !== null && me.endScript !== "") {
            me.machine.runScript(me.endScript, () => {
              errorCallback(msg);
            });
          }
          else {
            errorCallback(msg);
          }
        },
        progressCallback);
    };
    reader.onerror = evt => {
      if (typeof errorCallback === "function") {
        errorCallback(evt);
      }
    };
    let script = this.startScript;
    while (true) {
      let idx = script.indexOf("${filename}");
      if (idx >= 0) {
        let prefix = script.substring(0, idx);
        let suffix = script.substring(idx + 11);
        script = prefix + this.hostFileName + suffix;
      }
      else {
        break;
      }
    }
    this.machine.runScript(script, () => {
      reader.readAsArrayBuffer(me.localFile);
    });
  }
}
