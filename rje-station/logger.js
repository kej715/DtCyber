/*
 *  logger.js
 *
 *  This class provides methods for logging events.
 */

const fs = require("fs");

class Logger {

  constructor(name) {
    this.logStream = fs.createWriteStream(`${name}.log.txt`);
  }

  log(msg) {
    this.logStream.write(`${new Date().toLocaleString()} ${msg}\n`);
  }

  logData(label, data) {
    this.logStream.write(`${new Date().toLocaleString()} ${label}`);
    let n = 0;
    let text = "";
    for (const b of data) {
      if ((n % 16) === 0) {
        this.logStream.write(text);
        this.logStream.write("\n");
        text = "";
      }
      text += " ";
      if (b < 16) text += "0";
      text += b.toString(16);
      n += 1;
    }
    if (text.length > 1) {
      this.logStream.write(text);
      this.logStream.write("\n");
    }
  }
}

module.exports = Logger;
