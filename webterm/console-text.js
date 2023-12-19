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

const cosScreenTestDataBase64 = require('./testdata/cos')
const CyberConsoleText = require('./textconsole/js/cyber-console-text')

const BasicTerminalKitConsole = require('./textconsole/js/basic-terminal-kit')

function main() {
    let basicTerminalKitConsole = new BasicTerminalKitConsole()

    let consoleText = new CyberConsoleText(basicTerminalKitConsole)
    consoleText.createScreen();

    let index = 0;
    // Update the clock every second
    setInterval(function () {
        index++;
        if (index >= 3) {
            index = 0;
        }
        consoleText.clearScreenBuffer();
        const buffer = Buffer.from(cosScreenTestDataBase64[index], 'base64');
        consoleText.renderText(buffer);
        consoleText.updateScreen();
        // basicTerminalKitConsole.debugUpdateClock();
    }, 10);


}

main();