#!/usr/bin/env node
/*--------------------------------------------------------------------------
**
**  Copyright (c) 2025, Kevin Jordan
**
**  viking-console.js
**    Provides a text mode emulator of the CDC Viking 721 terminal, for
**    use as a NOS/VE console.
**
**--------------------------------------------------------------------------
*/

const fs            = require('fs');
const {program}     = require('commander');
const Machine       = require('./textconsole/js/machine-tcp');
const net           = require("net");
const VikingConsole = require('./textconsole/js/viking721');

function runVikingConsole(machineId, url, port, remote, pidFile) {
  let isRunning = true;

  fs.writeFileSync(pidFile, `${process.pid}\n`);

  let title = `${machineId} (${url}:${port})`;

  let client = null;

  const vikingConsole = new VikingConsole();
  vikingConsole.createScreen();

  const machine = new Machine(machineId, url, port);
  machine.setStreamingMode();
  machine.setReceivedDataHandler(data => {
    if (isRunning) {
      vikingConsole.renderText(data);
      if (client !== null) {
        try {
          client.write(data);
        }
        catch (err) {
          // ignore
        }
      }
    }
  });

  machine.setConnectListener(() => {
    vikingConsole.displayNotification(0, 0, `Connected to ${title}`);
  });

  machine.setDisconnectListener(() => {
    vikingConsole.displayNotification(0, 0,
      `Disconnected from ${title}.\n\n   Wait for automatic reconnect...\n\n   Check machine is running\n\n   Enter control-C to exit`);
  });

  machine.setReconnectStartListener(() => {
    vikingConsole.displayNotification(0, 0, `Waiting for connection to ${title}...\n\n   Please wait...`);
  });

  vikingConsole.setUplineDataSender(data => {
    machine.send(data);
  });

  vikingConsole.setShutdownListener(() => {
    isRunning = false;
    machine.closeConnection();
    vikingConsole.shutdown();
    // Let everything stop
    setTimeout(() => {
      // Use special exit to reset terminal
      vikingConsole.processExit();
    }, 500);
  });

  vikingConsole.displayNotification(0, 0, `Connecting to ${title}.\n\n   Please wait ...`);

  if (remote !== 0) {
    const server = net.createServer(c => {
      client = c;
      c.on('data', data => {
        machine.send(data.toString('utf8'));
      });
      c.on('end', () => {
        client = null;
      });
      c.on('error', err => {
        // ignore
      });
    });
    server.on('error', (err) => {
      throw err;
    });
    server.listen(remote);
  }

  machine.createConnection();
}

program
  .description('Text mode version of CDC Viking 721 terminal emulator for use as NOS/VE console.')
  .option('-i, --pid <pid>', 'Process ID file', 'viking-console.pid')
  .option('-m, --machine-id <id>', 'Machine ID', 'NOS/VE')
  .option('-p, --port <port>', 'The port number', (value) => {
    const parsedPort = parseInt(value, 10);
    if (isNaN(parsedPort) || parsedPort <= 0 || parsedPort > 65535) {
      throw new Error('Port must be an integer between 1 and 65535.');
    }
    return parsedPort;
  }, 6602)
  .option('-r, --remote <port>', 'The remote listening port number', (value) => {
    const parsedPort = parseInt(value, 10);
    if (isNaN(parsedPort) || parsedPort <= 0 || parsedPort > 65535) {
      throw new Error('Port must be an integer between 1 and 65535.');
    }
    return parsedPort;
  }, 0)
  .option('-u, --url <url>', 'The URL to connect to', '127.0.0.1')
  .parse(process.argv);

const options = program.opts();

runVikingConsole(options.machineId, options.url, options.port, options.remote, options.pid);
