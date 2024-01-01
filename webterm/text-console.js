#!/usr/bin/env node
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2023, Phil Claridge
**
**  text-console.js
**    Provides a text mode version of the Cyber Console for CLI use
**
**--------------------------------------------------------------------------
*/

/*
**  text-console.js
**    Text mode console for DtCyber using the webterm infrastructure
 */

const {program} = require('commander');
const CyberConsoleText = require('./textconsole/js/console-text');
const Machine = require('./textconsole/js/machine-tcp');

function runTextConsole(machineId, url, port, refresh) {
  let isRunning = true;

  let title = `${machineId} (${url}:${port},  refresh: ${refresh})`;

  let cyberConsole = new CyberConsoleText();
  cyberConsole.createScreen();

  const machine = new Machine(machineId, url, port);

  machine.setReceivedDataHandler(data => {
    if (isRunning) {
      cyberConsole.renderText(data);
    }
  });

  machine.setConnectListener(() => {
    cyberConsole.displayNotification(1, 128, 128, `Connected`);
    machine.send(new Uint8Array([0x80, refresh, 0x81]));
  });

  machine.setDisconnectListener(() => {
    cyberConsole.displayNotification(1, 128, 128,
      `Disconnected from ${title}.\n\n   Wait for automatic reconnect...\n\n   Check machine is running\n\n   Enter control-C to exit`);
  });

  machine.setReconnectStartListener(() => {
    cyberConsole.displayNotification(1, 128, 128, `Waiting for connection to ${title}...\n\n   Please wait...`);
  });

  cyberConsole.setUplineDataSender(data => {
    machine.send(data);
  });

  cyberConsole.setShutdownListener(() => {
    isRunning = false;
    machine.closeConnection();
    // cyberConsole.displayNotification(1, 128, 128,
    //     `Shutting down ...`);
    cyberConsole.shutdown();
    // Let everything stop
    setTimeout(() => {
      // Use special exit to reset terminal
      cyberConsole.processExit();
    }, 500);
  });

  cyberConsole.displayNotification(1, 128, 128, `Connecting to ${title}.\n\n   Please wait ...`);
  machine.createConnection();
}

function main() {
  program
    .description('Text mode version of the Cyber Console for CLI use')
    .option('-u, --url <url>', 'The URL to connect to', 'localhost')
    .option('-p, --port <port>', 'The port number', (value) => {
      const parsedPort = parseInt(value, 10);
      if (isNaN(parsedPort) || parsedPort <= 0 || parsedPort > 65535) {
        throw new Error('Port must be an integer between 1 and 65535.');
      }
      return parsedPort;
    }, 16612)
    .option('-r, --refresh <interval>', 'The refresh interval in 1/100 sec', (value) => {
      const parsedInterval = parseInt(value, 10);
      if (isNaN(parsedInterval) || parsedInterval < 1 || parsedInterval > 100) {
        throw new Error('Interval must be an integer between 1 and 100.');
      }
      return parsedInterval;
    }, 20)
    .option('-m, --machine-id <id>', 'Machine ID', 'nos287')
    .parse(process.argv);

  const options = program.opts();

  runTextConsole(options.machineId, options.url, options.port, options.refresh);
}

main();