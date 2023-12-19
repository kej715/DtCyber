const terminalKit = require("terminal-kit");

class BasicTerminalKitConsole {
    // Caution: terminal-kit's coordinates for a ScreenBuffer are 0-based,
    // not 1-based as stated in some documentation

    constructor() {
        //
        // Terminal instance
        //
        this.terminal = terminalKit.terminal;
        //
        // Console offsets
        //
        this.SCREEN_WIDTH_COLUMNS = 64;
        this.SCREEN_GAP_COLUMNS = 2;
        //
        // Screen control
        //
        this.screenBuffer = null;
        this.screenOffsetColumns = 0
    }


    setScreen(screenNumber) {
        this.screenOffsetColumns = screenNumber === 0 ? 0 : this.SCREEN_WIDTH_COLUMNS + this.SCREEN_GAP_COLUMNS;
    }

    // Core function: put
    put(x, y, char, fg = 'brightgreen', bg = 'black') {
        this.screenBuffer.put({x: x + this.screenOffsetColumns, y: y, attr: {color: fg, bgColor: bg}}, char);
        //this.screenBuffer.put({x: x + this.screenOffsetColumns, y: y}, char);
    }

    clearScreenBuffer() {
        // this.screenBuffer.clear();
        this.screenBuffer.fill({
            char: ' ',
            attr: {
                color: 'brightgreen',
                bgColor: 'black'
            }
        });
        // for (let x = 0; x <  this.terminal.width; x++) {
        //     for (let y = 0; y <  this.terminal.height; y++) {
        //         this.put(x, y, ' ', 'brightblue', 'grey')
        //     }
        // }

    }

    update() {
        this.screenBuffer.draw({delta: true});
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
        this.clearScreenBuffer();
        this.screenBuffer.draw({delta: false});
    }

    initialize() {
        this.createScreenBuffer(); // Create initial screen buffer

        // Event listener for terminal resize

        let me = this

        this.terminal.on('resize', function (width, height) {
            me.screenBuffer.resize({width: width, height: height, x: 0, y:0})
            me.clearScreenBuffer();
        });

        // terminal.grabInput({mouse: 'button'});

        this.terminal.on('key', function (key, matches, data) {
            switch (key) {
                // TODO fix up terminal to work for CDC
                case 'CTRL_C' :
                    me.terminal.clear();
                    me.terminal.hideCursor(false);
                    me.terminal.fullscreen(false);
                    process.exit();
                    break;
                default:
                    // Echo anything else
                    me.terminal.noFormat(
                        Buffer.isBuffer(data.code) ?
                            data.code :
                            String.fromCharCode(data.code)
                    );
                    break;
            }
        });

        // terminal.on('mouse', function (name, data) {
        //     terminal.moveTo(data.x, data.y);
        // });
    }
}

module.exports = BasicTerminalKitConsole
