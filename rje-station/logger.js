/*
 *  logger.js
 *
 *  This class provides methods for logging events.
 */

const fs = require("fs");

class Logger {

  constructor(name) {
    this.logStream = name === "-" ? process.stdout : fs.createWriteStream(`${name}.log.txt`);
  }

  log(msg) {
    this.logStream.write(`${new Date().toLocaleString()} ${msg}\n`);
  }

  logData(label, data) {
    let msg = `${new Date().toLocaleString()} ${label}`;
    let n = 0;
    let text = "";
    for (const b of data) {
      if ((n % 16) === 0) {
        msg = `${msg}${text}\n`;
        text = "";
      }
      text += " ";
      if (b < 16) text += "0";
      text += b.toString(16);
      n += 1;
    }
    this.logStream.write(`${msg}${text}\n`);
  }
}

module.exports = Logger;
