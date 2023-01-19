const utilities = {
  clearProgress: maxProgressLen => {
    let progress = `\r`;
    while (progress.length++ < maxProgressLen) progress += " ";
    process.stdout.write(`${progress}\r`);
    return Promise.resolve();
  },
  moveFile: (dtc, fileName, fromUi, toUi) => {
    return dtc.say(`Move ${fileName} from UI ${fromUi} to UI ${toUi}`)
    .then(() => dtc.runJob(12, 4, "opt/move-proc.job"))
    .then(() => dtc.dis([
      "GET,MOVPROC.",
      "PURGE,MOVPROC.",
      `MOVPROC,${fileName},${toUi}.`
    ], fromUi))
    .then(() => dtc.waitJob("MOVE"));
  },
  reportProgress: (byteCount, contentLength, maxProgressLen) => {
    let progress = `\r${new Date().toLocaleTimeString()}   Received ${byteCount}`;
    if (contentLength === -1) {
      progress += " bytes";
    }
    else {
      progress += ` of ${contentLength} bytes (${Math.round((byteCount / contentLength) * 100)}%)`;
    }
    process.stdout.write(progress)
    return (progress.length > maxProgressLen) ? progress.length : maxProgressLen;
  }
};

module.exports = utilities;
