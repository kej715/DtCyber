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

const CyberConsoleText = require('./textconsole/js/console-text')
const Machine = require('./textconsole/js/machine-tcp')

function main() {
    let url = 'localhost';
    let port = 16612;
    let refreshInterval = 20; // 1/100 sec
    let isRunning = true;

    const machineId = "nos287";
    this.title = `${machineId} (${url}:${port})`

    let cyberConsole = new CyberConsoleText()
    cyberConsole.createScreen();

    const machine = new Machine(machineId, url, port);

    machine.setReceivedDataHandler(data => {
        if (isRunning) {
            cyberConsole.renderText(data);
        }
    });

    machine.setConnectListener(() => {
        cyberConsole.displayNotification(1, 128, 128, `Connected`);
        machine.send(new Uint8Array([0x80, refreshInterval, 0x81]));
    });

    machine.setDisconnectListener(() => {
        cyberConsole.displayNotification(1, 128, 128,
            `Disconnected from ${this.title}.\n\n   Wait for automatic reconnect...\n\n   Check machine is running\n\n   Enter control-C to exit`);
    });

    machine.setReconnectStartListener(() => {
        cyberConsole.displayNotification(1, 128, 128, `Waiting for connection to ${this.title}.\n\n   Please wait...`);
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

    cyberConsole.displayNotification(1, 128, 128, `Connecting to ${this.title}.\n\n   Please wait ...`);
    machine.createConnection();
}

main();