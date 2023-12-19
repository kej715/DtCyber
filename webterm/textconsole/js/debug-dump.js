
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

export function dumpTestData() {
    const hexString = decodeBase64ToString(cosScreenTestDataBase64[0]);
    printHexDump(hexString);
}
