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
const CyberConsoleBase = require('./www/js/console-base');

class CyberConsoleText extends CyberConsoleBase {

    constructor() {
        super();

        this.xRatio = 1;
        this.yRatio = 1;
        this.fontWidths = [2, 8, 16, 32];
    }

    printChar(row, column, b) {
        console.log(">Term: " + b + "  Row: " + row + " Column: " + column)
    }
    
    drawPoint() {
        // Not supported
    }

    drawChar(b) {
        console.log(">Char: " + b + "  X: " + this.x + " Y: " + this.y)
        this.printChar(Math.floor(this.y/10), Math.floor(this.x/8), b)
    }

    setFont(b) {
        console.log(">Font: " + b)
    }

    setScreen(b) {
        console.log(">Screen: " + b)
    }

    updateScreen() {
        console.log(">Update")
    }

}


function decodeBase64ToString(base64String) {
    const buffer = Buffer.from(base64String, 'base64');
    return buffer.toString('hex');
}

function printHexDump(hexString) {
    const bytesPerLine = 32;
    for (let i = 0; i < hexString.length; i += bytesPerLine * 2) {
        const line = hexString.substring(i, i + bytesPerLine * 2);
        console.log(formatLine(line, bytesPerLine));
    }
}

function formatLine(line, bytesPerLine) {
    let hexPart = '';
    let asciiPart = '';
    for (let i = 0; i < line.length; i += 2) {
        const byte = line.substring(i, i + 2);
        hexPart += byte + ' ';

        // Convert byte to its ASCII character
        const charCode = parseInt(byte, 16);
        // Check if the character is unprintable
        const character = (charCode < 32 || charCode > 127) ? '.' : String.fromCharCode(charCode);
        asciiPart += character;
    }
    return hexPart.padEnd(bytesPerLine * 3) + ' ' + asciiPart;
}

function dumpTestData() {
    const hexString = decodeBase64ToString(cosScreenTestDataBase64[0]);
    printHexDump(hexString);
}

function main() {
    let consoleText = new CyberConsoleText()

    cosScreenTestDataBase64.forEach(base64String => {
        const buffer = Buffer.from(base64String, 'base64');
        consoleText.renderText(buffer)
        console.log("");
    });

}

main();