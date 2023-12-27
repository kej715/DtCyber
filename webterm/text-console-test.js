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


function main() {
    // const harFilePath = 'testdata/nos287_running.har';
    const harFilePath = 'testdata/nos287.har';
    const webSocketPackets = extractWebSocketPackets(harFilePath);
    let receivedMessagesBase64 = extractWebsocketMessages(webSocketPackets);

    let consoleText = new CyberConsoleText()
    consoleText.createScreen();

    let index = 0;
    let lastReceivedMessagesBase64 = ""
    // Update the clock every second
    setInterval(function () {
        index++;
        if (index >= receivedMessagesBase64.length) {
            index = 0;
        }
        consoleText.clearScreen();
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

main();