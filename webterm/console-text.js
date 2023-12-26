/*--------------------------------------------------------------------------
**
**  Copyright (c) 2023, Phil Claridge
**
**  console-text.js
**    Provides a text mode version of the Cyber Console for CLI use
**
**--------------------------------------------------------------------------
*/

/*
**  console-text.js
**    Text mode console for DtCyber using the webterm infrastructure
 */

const CyberConsoleText = require('./textconsole/js/cyber-console-text')

const BasicTerminalKitConsole = require('./textconsole/js/basic-terminal-kit')

const Machine = require('./textconsole/js/machine-tcp')

const fs = require('fs');

function extractWebSocketPackets(harFilePath) {
    const harContent = fs.readFileSync(harFilePath, 'utf8');
    const harData = JSON.parse(harContent);
    const packets = [];

    harData.log.entries.forEach(entry => {
        const isWebSocket = entry.response.headers.some(header =>
            header.name.toLowerCase() === 'upgrade' && header.value.toLowerCase() === 'websocket'
        );

        if (isWebSocket) {
            console.log(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>")
            packets.push(entry);
        }
    });

    return packets;
}

function extractWebsocketMessages(webSocketPackets) {
    let receivedMessagesBase64 = []
    webSocketPackets.forEach(packet => {
        let webSocketMessages = packet["_webSocketMessages"]
        webSocketMessages.forEach(message => {
            if (message["type"] === 'receive') {
                let messageDataBase64 = message['data'];
                receivedMessagesBase64.push(messageDataBase64)
            }
        });
    });
    return receivedMessagesBase64;
}


function main_test() {
    // const harFilePath = 'testdata/nos287_running.har';
    const harFilePath = 'testdata/nos287.har';
    const webSocketPackets = extractWebSocketPackets(harFilePath);
    let receivedMessagesBase64 = extractWebsocketMessages(webSocketPackets);

    let basicTerminalKitConsole = new BasicTerminalKitConsole()
    let consoleText = new CyberConsoleText(basicTerminalKitConsole)
    consoleText.createScreen();

    let index = 0;
    let lastReceivedMessagesBase64 = ""
    // Update the clock every second
    setInterval(function () {
        index++;
        if (index >= receivedMessagesBase64.length) {
            index = 0;
        }
        consoleText.clearScreenBuffer();
        let receivedMessagesBase64Element = receivedMessagesBase64[index];
        if (lastReceivedMessagesBase64 !== receivedMessagesBase64Element) {
            const buffer = Buffer.from(receivedMessagesBase64Element, 'base64');
            consoleText.renderText(buffer);
            consoleText.updateScreen();
            lastReceivedMessagesBase64 = receivedMessagesBase64Element;
            // console.log('X')
        } else {
            //console.log('.')
        }

    }, 200);
}


function main() {
    let url = 'localhost';
    let port = 16612;
    let refreshInterval = 20; // 1/100 sec

    let basicTerminalKitConsole = new BasicTerminalKitConsole()
    let cyberConsole = new CyberConsoleText(basicTerminalKitConsole)
    cyberConsole.createScreen();

    const machineId = "nos287";
    const machine = new Machine(machineId, url, port);
    machine.setTerminal(cyberConsole);
    machine.setReceivedDataHandler(data => {
        cyberConsole.renderText(data);
    });

    machine.setConnectListener(() => {
        machine.send(new Uint8Array([0x80, refreshInterval, 0x81]));
    });

    machine.createConnection();

    // const uplineDataSender = data => {
    //     machine.send(data);
    // };
    // cyberConsole.setUplineDataSender(uplineDataSender);
    //
    // machine.setDisconnectListener(() => {
    //     cyberConsole.displayNotification(1, 128, 128, `Disconnected from ${title}.\n\n   Press any key to reconnect ...`);
    //     cyberConsole.setUplineDataSender(data => {
    //         cyberConsole.reset();
    //         url = machine.createConnection();
    //         cyberConsole.setUplineDataSender(uplineDataSender);
    //     });
    // });
    // $(document).ajaxError((event, jqxhr, settings, thrownError) => {
    //     if (settings.url === url) {
    //         cyberConsole.displayNotification(1, 128, 128, `${jqxhr.responseText}\n\n   Failed to connect to ${title}\n\n   Press any key to try again ...`);
    //         cyberConsole.setUplineDataSender(data => {
    //             cyberConsole.reset();
    //             url = machine.createConnection();
    //             cyberConsole.setUplineDataSender(uplineDataSender);
    //         });
    //     }
    // });
    // $(window).bind("beforeunload", () => {
    //     machine.closeConnection();
    // });
}

main();