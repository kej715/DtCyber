/*--------------------------------------------------------------------------
**
**  Copyright (c) 2023, Kevin Jordan
**
**  console.js
**    This module provides common functionality to interface Cyber console.
**
**--------------------------------------------------------------------------
*/

/*
 *  CyberConsoleBase
 *
 *  This class provides the interface to decode the serial stream from the console.
 *
 */

class CyberConsoleBase {

    constructor() {
        //
        // Font sizes
        //
        this.DOT_FONT = 0;
        this.SMALL_FONT = 1;
        this.MEDIUM_FONT = 2;
        this.LARGE_FONT = 3;
        //
        // Console stream commands
        //
        this.CMD_SET_X_LOW = 0x80;
        this.CMD_SET_Y_LOW = 0x81;
        this.CMD_SET_X_HIGH = 0x82;
        this.CMD_SET_Y_HIGH = 0x83;
        this.CMD_SET_SCREEN = 0x84;
        this.CMD_SET_FONT_TYPE = 0x85;
        this.CMD_END_OF_FRAME = 0xff;
        //
        // Console states
        //
        this.ST_TEXT = 0;
        this.ST_COLLECT_SCREEN = 1;
        this.ST_COLLECT_FONT = 2;
        this.ST_COLLECT_X = 3;
        this.ST_COLLECT_Y = 4;
        //
        // Base Console emulation properties
        //
        // this.bgndColor         = "black";
        this.currentFont = this.SMALL_FONT;
        // this.fgndColor         = "lightgreen";
        // this.fontFamily        = "Lucida Typewriter";
        // this.fontHeights       = [0, 10, 20, 40];
        // this.offscreenCanvas   = null;
        // this.offscreenContext  = null;
        // this.onscreenCanvas    = null;
        this.state = this.ST_TEXT;
        this.x = 0;
        this.y = 0;
        this.xRatio = 1;
        this.yRatio = 1;
        //
        // Base font information
        //
        this.fontWidths = [2, 8, 16, 32];
    }

    //
    // Virtual functions
    //
    drawPoint() {
        console.log("Point")
    }

    drawChar(b) {
        console.log("Char: " + b)
    }

    setFont(font) {
        this.currentFont = font;
    }

    setScreen(b) {
        console.log("Screen: " + b)
    }

    updateScreen() {
        console.log("Update")
    }

    renderText(data) {

        if (typeof data === "string") {
            let ab = new Uint8Array(data.length);
            for (let i = 0; i < data.length; i++) {
                ab[i] = data.charCodeAt(i) & 0xff;
            }
            data = ab;
        }

        for (let i = 0; i < data.byteLength; i++) {
            let b = data[i];
            switch (this.state) {
                case this.ST_TEXT:
                    if (b < 0x80) {
                        if (this.currentFont === this.DOT_FONT) {
                            this.drawPoint();
                        } else {
                            if (b < 0x20) b = 0x20;
                            this.drawChar(b);
                            this.x += this.fontWidths[this.currentFont];
                        }
                    } else {
                        switch (b) {
                            case this.CMD_SET_X_LOW:
                                this.x = 0;
                                this.state = this.ST_COLLECT_X;
                                break;
                            case this.CMD_SET_Y_LOW:
                                this.y = 0;
                                this.state = this.ST_COLLECT_Y;
                                break;
                            case this.CMD_SET_X_HIGH:
                                this.x = 256;
                                this.state = this.ST_COLLECT_X;
                                break;
                            case this.CMD_SET_Y_HIGH:
                                this.y = 256;
                                this.state = this.ST_COLLECT_Y;
                                break;
                            case this.CMD_SET_SCREEN:
                                this.state = this.ST_COLLECT_SCREEN;
                                break;
                            case this.CMD_SET_FONT_TYPE:
                                this.state = this.ST_COLLECT_FONT;
                                break;
                            case this.CMD_END_OF_FRAME:
                                this.updateScreen();
                                break;
                            default:
                                // ignore the byte
                                break;
                        }
                    }
                    break;

                case this.ST_COLLECT_X:
                    console.log('Raw X: ' + ((this.x + b)))
                    this.x = Math.round((this.x + b) * this.xRatio);
                    this.state = this.ST_TEXT;
                    break;

                case this.ST_COLLECT_Y:
                    console.log('Raw Y: ' + ((this.y + b)) + ' ' + (0o777 - (this.y + b)))
                    this.y = Math.round((0o777 - (this.y + b)) * this.yRatio);
                    this.state = this.ST_TEXT;
                    break;

                case this.ST_COLLECT_SCREEN:
                    this.setScreen(b)
                    this.state = this.ST_TEXT;
                    break;

                case this.ST_COLLECT_FONT:
                    this.setFont(b);
                    this.state = this.ST_TEXT;
                    break;

                default:
                    // ignore byte
                    break;
            }
        }
    }
}

module.exports = CyberConsoleBase;