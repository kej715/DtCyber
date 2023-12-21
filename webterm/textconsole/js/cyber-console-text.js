let CyberConsoleBase = require("../../www/js/console-base");

class CyberConsoleText extends CyberConsoleBase {

    constructor(terminalDisplay) {
        super();
        this.terminalDisplay = terminalDisplay
        this.xRatio = 1;
        this.yRatio = 1;
        this.fontWidths = [2, 8, 16, 32];
    }

    drawPoint() {
        // Not supported
    }

    drawChar(b) {
        let xCharGrid = Math.floor(this.x/8);
        let yCharGrid = Math.floor(this.y/11);
        this.terminalDisplay.put(xCharGrid, yCharGrid, String.fromCharCode(b), 'brightgreen', 'black')
    }

    setFont(b) {
        // console.log(">Font: " + b)
    }

    setScreen(b) {
        this.terminalDisplay.setScreen(b);
    }

    updateScreen() {
        this.terminalDisplay.update();
    }

    clearScreenBuffer() {
        this.terminalDisplay.clearScreenBuffer()
    }

    createScreen()
    {
        this.terminalDisplay.initialize();
    }

    reset() {
        super.reset()
    }
}

module.exports = CyberConsoleText
