let CyberConsoleBase = require("../../www/js/console-base");
const terminalKit = require("terminal-kit");

class CyberConsoleText extends CyberConsoleBase {

    constructor() {
        super();
        this.xRatio = 1;
        this.yRatio = 1;
        this.fontWidths = [2, 8, 16, 32];
        this.uplineDataSender = null;
        this.charHeight = 11;
        this.charWidth = 8;
        //
        // Terminal instance
        //
        this.terminal = terminalKit.terminal;
        //
        // Console offsets
        //
        this.SCREEN_GAP      = 40;
        this.SCREEN_MARGIN   = 20;
        this.SCREEN_WIDTH_COLUMNS = 64;
        this.SCREEN_GAP_COLUMNS = 4;
        //
        // Screen control
        //
        this.screenBuffer = null;
        this.screenOffsetColumns = 0
        //
        // Callbacks
        //
        this.shutdownListener = null;
    }

    setShutdownListener(callback) {
        this.shutdownListener = callback;
    }

    put(x, y, char, fg = 'brightgreen', bg = 'black') {
        this.screenBuffer.put({x: x + this.screenOffsetColumns, y: y, attr: {color: fg, bgColor: bg}}, char);
    }

    createScreenBuffer() {
        // this.reset();
        this.terminal.fullscreen(true);
        this.terminal.hideCursor(true);

        this.screenBuffer = new terminalKit.ScreenBuffer({
            dst: this.terminal,
            width: this.terminal.width,
            height: this.terminal.height,
            wrap: false,
            noFill: false
        });
        this.clearScreen();
        this.screenBuffer.draw({delta: false});
    }


    drawPoint() {
        // Not supported
    }

    drawChar(b) {
        let xCharGrid = Math.floor(this.x / this.charWidth);
        let yCharGrid = Math.floor(this.y / this.charHeight);
        this.put(xCharGrid, yCharGrid, String.fromCharCode(b))
    }

    setFont(b) {
        // console.log(">Font: " + b)
    }

    setScreen(screenNumber) {
        this.screenOffsetColumns = screenNumber === 0 ? 0 : this.SCREEN_WIDTH_COLUMNS + this.SCREEN_GAP_COLUMNS;
    }

    updateScreen() {
        this.screenBuffer.draw({delta: true});
    }

    clearScreen() {
        this.screenBuffer.fill({
            char: ' ',
            attr: {
                color: 'brightgreen',
                bgColor: 'black'
            }
        });
    }

    shutdown() {
        this.terminal.clear();
        this.terminal.fullscreen(false);
        this.terminal.hideCursor(false);
    }

    processExit() {
        // Ensures console cleaned up
        this.terminal.processExit(0);
    }
    createScreen() {
        this.createScreenBuffer(); // Create initial screen buffer

        // Event listener for terminal resize

        let me = this

        this.terminal.on('resize', function (width, height) {
            me.screenBuffer.resize({width: width, height: height, x: 0, y: 0})
            me.clearScreen();
            me.screenBuffer.draw({delta: false});
        });

        this.terminal.grabInput({mouse: 'button'});

        this.terminal.on('key', function (key, matches, data) {
            switch (key) {
                // TODO fix up terminal to work for CDC
                case 'CTRL_C' :
                    if (me.shutdownListener) {
                        me.shutdownListener();
                    }
                    break;
                default:
                    // Echo anything else
                    // me.terminal.noFormat(
                    //     Buffer.isBuffer(data.code) ?
                    //         data.code :
                    //         String.fromCharCode(data.code)
                    // );
                    break;
            }
        });

        this.terminal.on('mouse', function (name, data) {
        });
    }

    reset() {
        super.reset()
    }

    setUplineDataSender(callback) {
        this.uplineDataSender = callback;
    }
}

module.exports = CyberConsoleText
