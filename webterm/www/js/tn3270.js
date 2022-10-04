/*--------------------------------------------------------------------------
**
**  Copyright (c) 2018, Kevin Jordan
**
**  tn3270.js
**    This class implements an IBM 3270 terminal emulator.
**
**--------------------------------------------------------------------------
*/

class Tn3270 {

  constructor() {

    this.isDebug  = false;
    this.isTn3270 = true;

    //
    // Telnet protocol states
    //
    this.ST_DATA       = 0;
    this.ST_TN_IAC     = 1;
    this.ST_TN_CR      = 2;
    this.ST_TN_WILL    = 3;
    this.ST_TN_WONT    = 4;
    this.ST_TN_DO      = 5;
    this.ST_TN_DONT    = 6;
    this.ST_TN_SB      = 7;
    this.ST_TN_SB_IAC  = 8;

    //
    // Telnet protocol elements
    //
    this.EOR           = 239; // End of Record
    this.SE            = 240; // End of sub-negotiation
    this.NOP           = 241;
    this.DATA_MARK     = 242;
    this.BREAK         = 243;
    this.IP            = 244; // Interrupt Process
    this.AO            = 245; // Abort Output
    this.AYT           = 246; // Are You There?
    this.EC            = 247; // Erase Character
    this.EL            = 248; // Erase Line
    this.GA            = 249; // Go Ahead
    this.SB            = 250; // Start Sub-negotiation
    this.WILL          = 251;
    this.WONT          = 252;
    this.DO            = 253;
    this.DONT          = 254;
    this.IAC           = 255;

    //
    // Telnet protocol options
    //
    this.OPT_BINARY    = 0;
    this.OPT_ECHO      = 1;
    this.OPT_SGA       = 3;
    this.OPT_TERM_TYPE = 24;
    this.OPT_EOR       = 25;

    this.TERM_TYPE_IS   = 0;
    this.TERM_TYPE_SEND = 1;

    //
    // 3270 command codes
    //
    this.CC_W           = 0xf1; // Write
    this.CC_EW          = 0xf5; // Erase/Write
    this.CC_EWA         = 0x7e; // Erase/Write Alternate
    this.CC_RB          = 0xf2; // Read Buffer
    this.CC_RM          = 0xf6; // Read Modified
    this.CC_RMA         = 0x63; // Read Modified All
    this.CC_EAU         = 0x6f; // Erase All Unprotected
    this.CC_WSF         = 0xf3; // Write Structured Field

    //
    // 3270 Order codes
    //
    this.ORDER_SF       = 0x1d; // Start Field
    this.ORDER_SFE      = 0x29; // Start Field Extended
    this.ORDER_SBA      = 0x11; // Set Buffer Address
    this.ORDER_SA       = 0x28; // Set Attribute
    this.ORDER_MF       = 0x2c; // Modify Field
    this.ORDER_IC       = 0x13; // Insert Cursor
    this.ORDER_PT       = 0x05; // Program Tab
    this.ORDER_RA       = 0x3c; // Repeat to Address
    this.ORDER_EUA      = 0x12; // Erase Unprotected to Address
    this.ORDER_GE       = 0x08; // Graphic Escape

    //
    // 3270 Format Control Order codes
    //
    this.ORDER_NUL      = 0x00; // Blank, suppressed on Read Modified
    this.ORDER_SUB      = 0x3f; // Solid circle
    this.ORDER_DUP      = 0x1c; // Overscore asterisk
    this.ORDER_FM       = 0x1e; // Overscore semicolon
    this.ORDER_FF       = 0x0c; // Blank
    this.ORDER_CR       = 0x0d; // Blank
    this.ORDER_NL       = 0x15; // Blank
    this.ORDER_EM       = 0x19; // Blank
    this.ORDER_EO       = 0xff; // Blank

    //
    // 3270 AID (Attention ID) codes
    //
    this.AID_NAG        = 0x60; // No AID Generated
    this.AID_SF         = 0x88; // Structured Field
    this.AID_RP         = 0x61; // Read Partition
    this.AID_TA         = 0x7f; // Trigger Action
    this.AID_REQ        = 0xf0; // Test REQ and SYS Req
    this.AID_PF1        = 0xf1; // PF1  Key
    this.AID_PF2        = 0xf2; // PF2  Key
    this.AID_PF3        = 0xf3; // PF3  Key
    this.AID_PF4        = 0xf4; // PF4  Key
    this.AID_PF5        = 0xf5; // PF5  Key
    this.AID_PF6        = 0xf6; // PF6  Key
    this.AID_PF7        = 0xf7; // PF7  Key
    this.AID_PF8        = 0xf8; // PF8  Key
    this.AID_PF9        = 0xf9; // PF9  Key
    this.AID_PF10       = 0x7a; // PF10 Key
    this.AID_PF11       = 0x7b; // PF11 Key
    this.AID_PF12       = 0x7c; // PF12 Key
    this.AID_PF13       = 0xc1; // PF13 Key
    this.AID_PF14       = 0xc2; // PF14 Key
    this.AID_PF15       = 0xc3; // PF15 Key
    this.AID_PF16       = 0xc4; // PF16 Key
    this.AID_PF17       = 0xc5; // PF17 Key
    this.AID_PF18       = 0xc6; // PF18 Key
    this.AID_PF19       = 0xc7; // PF19 Key
    this.AID_PF20       = 0xc8; // PF20 Key
    this.AID_PF21       = 0xc9; // PF21 Key
    this.AID_PF22       = 0x4a; // PF22 Key
    this.AID_PF23       = 0x4b; // PF23 Key
    this.AID_PF24       = 0x4c; // PF24 Key
    this.AID_PA1        = 0x6c; // PA1  Key
    this.AID_PA2        = 0x6e; // PA2  Key
    this.AID_PA3        = 0x6b; // PA3  Key
    this.AID_CLEAR      = 0x6d; // Clear Key
    this.AID_CLEAR_PART = 0x6a; // Clear Partition Key
    this.AID_ENTER      = 0x7d; // Enter Key
    this.AID_PEN        = 0x7e; // Selector Pen

    //
    // Function key to AID map
    //
    this.fKeyToAidMap = {
      "F1" : this.AID_PF1,  "F2" : this.AID_PF2,  "F3" : this.AID_PF3,  "F4" : this.AID_PF4,
      "F5" : this.AID_PF5,  "F6" : this.AID_PF6,  "F7" : this.AID_PF7,  "F8" : this.AID_PF8,
      "F9" : this.AID_PF9,  "F10": this.AID_PF10, "F11": this.AID_PF11, "F12": this.AID_PF12,
      "SF1": this.AID_PF13, "SF2": this.AID_PF14, "SF3": this.AID_PF15, "SF4": this.AID_PF16,
      "SF5": this.AID_PF17, "SF6": this.AID_PF18, "SF7": this.AID_PF19, "SF8": this.AID_PF20,
      "SF9": this.AID_PF21, "SF10":this.AID_PF22, "SF11":this.AID_PF23, "SF12":this.AID_PF24
    };

    //
    // WCC bit masks
    //
    this.WCC_RESET         = 0x40;
    this.WCC_START_PRINTER = 0x08;
    this.WCC_SOUND_ALARM   = 0x04;
    this.WCC_KB_RESTORE    = 0x02;
    this.WCC_RESET_MDT     = 0x01;

    //
    // Field Attribute bit masks
    //
    this.FA_MDT            = 0x01;  // Modified data tag, 1 = modified by user or application
    this.FA_DISPLAY_MASK   = 0x0c;
    this.FA_DISPLAY_NO_PEN =    0;       // 00 display/not pen detectable
    this.FA_DISPLAY_PEN    =    (1<<2);  // 01 display/pen detectable
    this.FA_INTENSE        =    (2<<2);  // 10 intensified display/pen detectable
    this.FA_NONDISPLAY     =    (3<<2);  // 11 nondisplay/not pen detectable
    this.FA_NUMERIC        = 0x10;
    this.FA_PROTECTED      = 0x20;

    //
    // Structured field codes required for SAA support
    //
    this.SFC_READ_PARTITION    = 0x01;
    this.SFC_ERASE_RESET       = 0x03;
    this.SFC_OUTBOUND_3270DS   = 0x40;
    this.SFC_QUERY_REPLY       = 0x81;

    //
    // Read Partition type codes
    //
    this.RPT_QUERY             = 0x02;
    this.RPT_QUERY_LIST        = 0x03;

    //
    // Query reply codes required for SAA support
    //
    this.QR_SUMMARY            = 0x80;
    this.QR_USABLE_AREA        = 0x81;
    this.QR_CHARACTER_SETS     = 0x85;
    this.QR_IMPLICIT_PARTITION = 0xa6;
    this.QR_NULL               = 0xff;

    //
    // Additional query reply codes supported
    //
    this.QR_ALPHANUMERIC_PARTITIONS = 0x84;
    this.QR_COLOR                   = 0x86;
    this.QR_HIGHLIGHT               = 0x87;
    this.QR_REPLY_MODES             = 0x88;
    this.QR_RPQ_NAMES               = 0xa1;

    //
    // Read buffer/partition reply modes
    //
    this.RM_FIELD              = 0x00;
    this.RM_EXTENDED_FIELD     = 0x01;
    this.RM_CHARACTER          = 0x02;

    //
    // Terminal emulation properties
    //
    this.canvas         = null;
    this.command        = [];
    this.context        = null;
    this.doEcho         = false;
    this.doEor          = false;
    this.fontFamily     = "ibm3270";
    this.fontHeight     = 16;
    this.fontWidth      = 10;
    this.is16BitMode    = false;
    this.isTerminalWait = false;
    this.replyMode      = this.RM_FIELD;
    this.state          = this.ST_DATA;
    this.termType       = "IBM-3279-4-E@MOD4";

    this.currentCharAttrs = {
      color: 0,
      highlight: 0
    };

    this.terminalStatusHandlers = [];

    //
    // Color scheme
    //
    this.defaultBgndColor = "black";
    this.defaultFgndColor = "lightgreen";
    this.intensifiedColor = "cyan";
    this.selectBgndColor  = "dimgray";

    this.colors           = new Array(256);
    this.colors[0]        = this.defaultFgndColor;
    this.colors[0xf1]     = "deepskyblue";
    this.colors[0xf2]     = "red";
    this.colors[0xf3]     = "pink";
    this.colors[0xf4]     = this.defaultFgndColor;
    this.colors[0xf5]     = "turquoise";
    this.colors[0xf6]     = "yellow";
    this.colors[0xf7]     = "white";
    this.colors[0xf8]     = "black";
    this.colors[0xf9]     = "darkblue";
    this.colors[0xfa]     = "orange";
    this.colors[0xfb]     = "purple";
    this.colors[0xfc]     = "palegreen";
    this.colors[0xfd]     = "paleturquoise";
    this.colors[0xfe]     = "gray";
    this.colors[0xff]     = "white";

    this.protHighlightColorIndex   = 0xff;
    this.protNormalColorIndex      = 0xf1;
    this.unprotHighlightColorIndex = 0xf2;
    this.unprotNormalColorIndex    = 0xf4;

    //
    // Character highlight flags
    //
    this.HL_INTENSE    = 0x01;
    this.HL_REVERSE    = 0x02;
    this.HL_UNDERSCORE = 0x04;
    this.HL_BLINK      = 0x08;

    //
    // ASCII to EBCDIC character code translation table
    //
    this.asciiToEbcdic = [
    /* 00-07 */  0x00,    0x01,    0x02,    0x03,    0x37,    0x2d,    0x2e,    0x2f,
    /* 08-0f */  0x16,    0x05,    0x25,    0x0b,    0x0c,    0x0d,    0x0e,    0x0f,
    /* 10-17 */  0x10,    0x11,    0x12,    0x13,    0x3c,    0x3d,    0x32,    0x26,
    /* 18-1f */  0x18,    0x19,    0x3f,    0x27,    0x1c,    0x1d,    0x1e,    0x1f,
    /* 20-27 */  0x40,    0x5a,    0x7f,    0x7b,    0x5b,    0x6c,    0x50,    0x7d,
    /* 28-2f */  0x4d,    0x5d,    0x5c,    0x4e,    0x6b,    0x60,    0x4b,    0x61,
    /* 30-37 */  0xf0,    0xf1,    0xf2,    0xf3,    0xf4,    0xf5,    0xf6,    0xf7,
    /* 38-3f */  0xf8,    0xf9,    0x7a,    0x5e,    0x4c,    0x7e,    0x6e,    0x6f,
    /* 40-47 */  0x7c,    0xc1,    0xc2,    0xc3,    0xc4,    0xc5,    0xc6,    0xc7,
    /* 48-4f */  0xc8,    0xc9,    0xd1,    0xd2,    0xd3,    0xd4,    0xd5,    0xd6,
    /* 50-57 */  0xd7,    0xd8,    0xd9,    0xe2,    0xe3,    0xe4,    0xe5,    0xe6,
    /* 58-5f */  0xe7,    0xe8,    0xe9,    0xad,    0xe0,    0xbd,    0x5f,    0x6d,
    /* 60-67 */  0x79,    0x81,    0x82,    0x83,    0x84,    0x85,    0x86,    0x87,
    /* 68-6f */  0x88,    0x89,    0x91,    0x92,    0x93,    0x94,    0x95,    0x96,
    /* 70-77 */  0x97,    0x98,    0x99,    0xa2,    0xa3,    0xa4,    0xa5,    0xa6,
    /* 78-7f */  0xa7,    0xa8,    0xa9,    0xc0,    0x4f,    0xd0,    0xa1,    0x07,

    /* 80-87 */  0x20,    0x01,    0x02,    0x03,    0x37,    0x2d,    0x2e,    0x2f,
    /* 88-8f */  0x16,    0x05,    0x25,    0x0b,    0x0c,    0x0d,    0x0e,    0x0f,
    /* 90-97 */  0x10,    0x11,    0x12,    0x13,    0x3c,    0x3d,    0x32,    0x26,
    /* 98-9f */  0x18,    0x19,    0x3f,    0x27,    0x1c,    0x1d,    0x1e,    0x1f,
    /* a0-a7 */  0x40,    0x5a,    0x4a,    0xb1,    0x5b,    0xb2,    0x6a,    0x7d,
    /* a8-af */  0x4d,    0x5d,    0x5c,    0x4e,    0x6b,    0x60,    0x4b,    0x61,
    /* b0-b7 */  0xf0,    0xf1,    0xf2,    0xf3,    0xf4,    0xf5,    0xf6,    0xf7,
    /* b8-bf */  0xf8,    0xf9,    0x7a,    0x5e,    0x4c,    0x7e,    0x6e,    0x6f,
    /* c0-c7 */  0x7c,    0xc1,    0xc2,    0xc3,    0xc4,    0xc5,    0xc6,    0xc7,
    /* c8-cf */  0xc8,    0xc9,    0xd1,    0xd2,    0xd3,    0xd4,    0xd5,    0xd6,
    /* d0-d7 */  0xd7,    0xd8,    0xd9,    0xe2,    0xe3,    0xe4,    0xe5,    0xe6,
    /* d8-df */  0xe7,    0xe8,    0xe9,    0xad,    0xe0,    0xbd,    0x5f,    0x6d,
    /* e0-e7 */  0x79,    0x81,    0x82,    0x83,    0x84,    0x85,    0x86,    0x87,
    /* e8-ef */  0x88,    0x89,    0x91,    0x92,    0x93,    0x94,    0x95,    0x96,
    /* f0-f7 */  0x97,    0x98,    0x99,    0xa2,    0xa3,    0xa4,    0xa5,    0xa6,
    /* f8-ff */  0xa7,    0xa8,    0xa9,    0xc0,    0x4f,    0xd0,    0xa1,    0x07
    ];

    //
    // EBCDIC to ASCII character code translation table
    //
    this.ebcdicToAscii = [
    /* 00-07 */  0x00,    0x01,    0x02,    0x03,    0x1a,    0x09,    0x1a,    0x7f,
    /* 08-0f */  0x1a,    0x1a,    0x1a,    0x0b,    0x0c,    0x0d,    0x0e,    0x0f,
    /* 10-17 */  0x10,    0x11,    0x12,    0x13,    0x1a,    0x1a,    0x08,    0x1a,
    /* 18-1f */  0x18,    0x19,    0x1a,    0x1a,    0x1c,    0x1d,    0x1e,    0x1f,
    /* 20-27 */  0x80,    0x1a,    0x1a,    0x1a,    0x1a,    0x0a,    0x17,    0x1b,
    /* 28-2f */  0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x05,    0x06,    0x07,
    /* 30-37 */  0x1a,    0x1a,    0x16,    0x1a,    0x1a,    0x1a,    0x1a,    0x04,
    /* 38-3f */  0x1a,    0x1a,    0x1a,    0x1a,    0x14,    0x15,    0x1a,    0x1a,
    /* 40-47 */  0x20,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* 48-4f */  0x1a,    0x1a,    0xa2,    0x2e,    0x3c,    0x28,    0x2b,    0x7c,
    /* 50-57 */  0x26,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* 58-5f */  0x1a,    0x1a,    0x21,    0x24,    0x2a,    0x29,    0x3b,    0x5e,
    /* 60-67 */  0x2d,    0x2f,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* 68-6f */  0x1a,    0x1a,    0xa6,    0x2c,    0x25,    0x5f,    0x3e,    0x3f,
    /* 70-77 */  0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* 78-7f */  0x1a,    0x60,    0x3a,    0x23,    0x40,    0x27,    0x3d,    0x22,

    /* 80-87 */  0x1a,    0x61,    0x62,    0x63,    0x64,    0x65,    0x66,    0x67,
    /* 88-8f */  0x68,    0x69,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* 90-97 */  0x1a,    0x6a,    0x6b,    0x6c,    0x6d,    0x6e,    0x6f,    0x70,
    /* 98-9f */  0x71,    0x72,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* a0-a7 */  0x1a,    0x7e,    0x73,    0x74,    0x75,    0x76,    0x77,    0x78,
    /* a8-af */  0x79,    0x7a,    0x1a,    0x1a,    0x1a,    0x5b,    0x1a,    0x1a,
    /* b0-b7 */  0x5e,    0xa3,    0xa5,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* b8-bf */  0x1a,    0x1a,    0x5b,    0x5d,    0x1a,    0x5d,    0x1a,    0x1a,
    /* c0-c7 */  0x7b,    0x41,    0x42,    0x43,    0x44,    0x45,    0x46,    0x47,
    /* c8-cf */  0x48,    0x49,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* d0-d7 */  0x7d,    0x4a,    0x4b,    0x4c,    0x4d,    0x4e,    0x4f,    0x50,
    /* d8-df */  0x51,    0x52,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* e0-e7 */  0x5c,    0x1a,    0x53,    0x54,    0x55,    0x56,    0x57,    0x58,
    /* e8-ef */  0x59,    0x5a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,
    /* f0-f7 */  0x30,    0x31,    0x32,    0x33,    0x34,    0x35,    0x36,    0x37,
    /* f8-fF */  0x38,    0x39,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a,    0x1a
    ];

    //
    // 12-bit addressing table
    //
    this.addressXlation = [
    /* 00-0f */ 0x40,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
    /* 10-1f */ 0x50,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
    /* 20-2f */ 0x60,0x61,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
    /* 30-3f */ 0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f
    ];
  }

  toHex(value) {
    let result = [];
    if (Array.isArray(value)) {
      for (let i = 0; i < value.length; i++) {
        let byte = value[i];
        result.push((byte < 16) ? "0" + byte.toString(16) : byte.toString(16));
      }
    }
    else {
      for (let i = 0; i < value.byteLength; i++) {
        let byte = value[i];
        result.push((byte < 16) ? "0" + byte.toString(16) : byte.toString(16));
      }
    }
    return result;
  }

  debug(message, bytes) {
    if (this.isDebug) {
      if (bytes) {
        console.log(`${new Date().toISOString()} ${message}: [${this.toHex(bytes).join(" ")}]`);
      }
      else {
        console.log(`${new Date().toISOString()} ${message}`);
      }
    }
  }

  debugCmd(cmd, wcc) {
    if (this.isDebug) {
      let msg = `....${cmd}`;
      if (wcc) {
        let fns = [];
        if ((wcc & this.WCC_RESET) != 0)         fns.push("Reset");
        if ((wcc & this.WCC_START_PRINTER) != 0) fns.push("Start Printer");
        if ((wcc & this.WCC_SOUND_ALARM) != 0)   fns.push("Sound Alarm");
        if ((wcc & this.WCC_KB_RESTORE) != 0)    fns.push("Keyboard Restore");
        if ((wcc & this.WCC_RESET_MDT) != 0)     fns.push("Reset MDT");
        console.log(`${new Date().toISOString()} ....${cmd} (${fns.join(",")})`);
      }
      else {
        console.log(`${new Date().toISOString()} ....${cmd}`);
      }
    }
  }

  setDebug(db) {
    this.isDebug = db;
  }

  setFont(height) {
    this.font = `${height}px ${this.fontFamily}`;
    let testLine = "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM";
    this.fontHeight = height + 1;
    let cnv = document.createElement("canvas");
    let ctx = cnv.getContext("2d");
    cnv.width = testLine.length * 15;
    cnv.height = this.fontHeight;
    ctx.font = this.font;
    this.fontWidth = Math.round(ctx.measureText(testLine).width / testLine.length);
  }

  setKeyboardInputHandler(callback) {
    this.keyboardInputHandler = callback;
  }

  addTerminalStatusHandler(callback) {
    this.terminalStatusHandlers.push(callback);
  }

  removeTerminalStatusHandler(callback) {
    let i = 0;
    while (i < this.terminalStatusHandlers.length) {
      if (this.terminalStatusHandlers[i] === callback) {
        this.terminalStatusHandlers.splice(i, 1);
        break;
      }
      i += 1;
    }
  }

  setTerminalType(termType) {
    this.termType = termType;
  }

  notifyTerminalStatus() {
    const status = {
      isTerminalWait: this.isTerminalWait,
      cursorRow: Math.floor(this.cursorAddress / this.cols) + 1,
      cursorCol: this.cursorAddress % this.cols + 1
    };
    if (this.isDebug) {
      this.debug(`Notify terminal status, locked ${status.isTerminalWait}, row ${status.cursorRow}, col ${status.cursorCol}`);
    }
    for (let handler of this.terminalStatusHandlers) {
      handler(status);
    }
  }

  getBuffer() {
    return this.buffer;
  }

  getCanvas() {
    return this.canvas;
  }

  getWidth() {
    return this.fontWidth * this.cols;
  }

  processKeyboardEvent(keyStr, shiftKey, ctrlKey, altKey) {
    if (this.isTerminalWait
        && keyStr !== "Escape"
        && !(keyStr === "1" && ctrlKey)) {
      return;
    }
    let field = this.getFieldForAddress(this.cursorAddress);
    //
    // Handle non-special keys
    //
    if (keyStr.length < 2) { // non-special key
      if (ctrlKey) {
        switch (keyStr) {
        case "c": // Ctrl-C
        case "C":
          this.resetCurrentCharAttrs();
          this.sendAID(this.AID_CLEAR);
          break;
        case "h": // Ctrl-H <BS>
        case "H":
          if ((field.attribute & this.FA_PROTECTED) === 0) {
            this.eraseChar();
          }
          break;
        case "i": // Ctrl-I <HT>
        case "I":
          this.hideCursor();
          this.skipToNextField();
          this.showCursor();
          break;
        case "j": // Ctrl-J <LF>
        case "J":
        case "m": // Ctrl-M <CR>
        case "M":
          this.sendModifiedFields(this.AID_ENTER);
          break;
        case "s": // Ctrl-S
        case "S":
          this.sendAID(this.AID_REQ); // SysReq
          break;
        case "1":
          this.sendAID(this.AID_PA1);
          break;
        case "2":
          this.sendAID(this.AID_PA2);
          break;
        case "3":
          this.sendAID(this.AID_PA3);
          break;
        default:   // ignore
          break;
        }
      }
      else {
        let c = keyStr.charCodeAt(0);
        if (c >= 0x20 && c < 0x7f
            && (field.attribute & this.FA_PROTECTED) === 0
            && this.cursorAddress >  field.address
            && this.cursorAddress <= field.address + field.size) {
          this.hideCursor();
          this.echoChar(c);
          this.buffer[this.cursorAddress] = this.asciiToEbcdic[c];
          this.buffer[field.address] |= this.FA_MDT;
          this.moveCursor(1);
          this.showCursor();
        }
      }
    }
    //
    // Handle special keys
    //
    else {

      switch (keyStr) {
      case "Backspace":
      case "Delete":
        if ((field.attribute & this.FA_PROTECTED) === 0) {
          this.eraseChar();
        }
        break;
      case "Enter":
        this.sendModifiedFields(this.AID_ENTER);
        break;
      case "Escape":
        this.sendAID(this.AID_PA1);
        break;
      case "Tab":
        this.hideCursor();
        this.skipToNextField();
        this.showCursor();
        break;
      case "ArrowDown":
        this.hideCursor();
        this.moveCursor(this.cols);
        this.showCursor();
        break;
      case "ArrowLeft":
        this.hideCursor();
        this.moveCursor(-1);
        this.showCursor();
        break;
      case "ArrowRight":
        this.hideCursor();
        this.moveCursor(1);
        this.showCursor();
        break;
      case "ArrowUp":
        this.hideCursor();
        this.moveCursor(-this.cols);
        this.showCursor();
        break;
      case "F1":
      case "F2":
      case "F3":
      case "F4":
      case "F5":
      case "F6":
      case "F7":
      case "F8":
      case "F9":
      case "F10":
      case "F11":
      case "F12":
        if (shiftKey) {
          this.sendAID(this.fKeyToAidMap[`S${keyStr}`]);
        }
        else {
          this.sendAID(this.fKeyToAidMap[keyStr]);
        }
        break;
      default: // ignore the key
        break;
      }
    }
  }

  createScreen(fontSize, rows, cols) {
    this.rows = rows;
    this.cols = cols;
    this.buffer = new Uint8Array(rows * cols);
    this.charAttrs = [];
    while (this.charAttrs.length < this.buffer.length) {
      this.charAttrs.push({
        color: 0,
        highlight: 0
      });
    }
    this.bufferAddress = 0;
    this.cursorAddress = 0;
    this.canvas = document.createElement("canvas");
    this.context = this.canvas.getContext("2d");
    this.fieldAddresses = [];
    this.setFont(fontSize);
    this.canvas.width = this.fontWidth * this.cols;
    this.canvas.height = this.fontHeight * this.rows;
    this.context.font = this.font;
    this.context.textBaseline = "top";
    this.canvas.style.border = "1px solid black";
    this.reset();
    this.canvas.setAttribute("tabindex", 0);
    this.canvas.setAttribute("contenteditable", "true");

    const me = this;

    this.canvas.addEventListener("click", () => {
      me.canvas.focus();
    });

    this.canvas.addEventListener("mouseover", () => {
      me.canvas.focus();
    });

    this.canvas.addEventListener("keydown", evt => {
      evt.preventDefault();
      me.processKeyboardEvent(evt.key, evt.shiftKey, evt.ctrlKey, evt.altKey);
    });
  }

  lookupColor(index) {
    return this.colors[index] || this.defaultFgndColor;
  }

  resetCurrentCharAttrs() {
    this.currentCharAttrs = {
      color: 0,
      highlight: 0
    };
  }

  resetCharAttrs(address) {
    this.charAttrs[address] = {
      color: 0,
      highlight: 0
    };
    return this.charAttrs[address];
  }

  resetAllCharAttrs() {
    for (let address = 0; address < this.charAttrs.length; address++) {
      this.resetCharAttrs(address);
    }
  }

  reset() {
    this.bufferAddress       = 0;
    this.cursorAddress       = 0;
    this.fieldAddresses      = [];
    this.replyMode           = this.RM_FIELD;
    this.buffer.fill(0);
    this.resetCurrentCharAttrs();
    this.resetAllCharAttrs();
    let width = this.cols * this.fontWidth;
    let height = this.rows * this.fontHeight;
    this.context.fillStyle = this.defaultBgndColor;
    this.context.clearRect(0, 0, width, height);
    this.context.fillRect(0, 0, width, height);
  }

  drawChar(ch, address, fgndColor, bgndColor, highlights) {
    if (address < this.buffer.length) {
      let row = Math.floor(address / this.cols);
      let col = address % this.cols;
      let x = col * this.fontWidth;
      let y = row * this.fontHeight;
      if (highlights && highlights !== 0) {
        if ((highlights & this.HL_INTENSE) !== 0) {
          fgndColor = this.intensifiedColor;
        }
        if ((highlights & this.HL_REVERSE) !== 0) {
          let c = bgndColor;
          bgndColor = fgndColor;
          fgndColor = c;
        }
      }
      this.context.fillStyle = bgndColor;
      this.context.clearRect(x, y, this.fontWidth, this.fontHeight);
      this.context.fillRect(x, y, this.fontWidth, this.fontHeight);
      this.context.fillStyle = fgndColor;
      this.context.fillText(ch, x, y);
      if ((highlights & this.HL_UNDERSCORE) !== 0) {
        y = y + this.fontHeight - 1;
        this.context.moveTo(x, y);
        this.context.lineTo(x + this.fontWidth - 1, y);
        this.context.strokeStyle = fgndColor;
        this.context.stroke();
      }
    }
  }

  setCharAttrs(addr) {
    let attrs = this.charAttrs[addr];
    attrs.color = this.currentCharAttrs.color;
    attrs.highlight = this.currentCharAttrs.highlight;
    return attrs;
  }

  calculateHighlights(field, attrs) {
    let highlights = ((field.attribute & this.FA_DISPLAY_MASK) === this.FA_INTENSE) ? this.HL_INTENSE : 0;
    switch (attrs.highlight) {
    case 0x00: // default
    case 0xf0: // normal highlight
      // no flag to set
      break;
    case 0xf1:
     highlights |= this.HL_BLINK;
     break;
    case 0xf2:
     highlights |= this.HL_REVERSE;
     break;
    case 0xf4:
     highlights |= this.HL_UNDERSCORE;
     break;
    case 0xf8:
     highlights |= this.HL_INTENSE;
     break;
    default:
      console.log(`Unsupported character highlight value 0x${attrs.highlight.toString(16)} ignored`);
      break;
    }
    return highlights;
  }

  writeChar(e) {
    let field = this.getFieldForAddress(this.bufferAddress);
    if (field.address === this.bufferAddress) {
      this.fieldAddresses.splice(this.fieldAddresses.indexOf(field.address), 1);
      field = this.getFieldForAddress(this.bufferAddress);
    }
    let a = this.ebcdicToAscii[e];
    if (a < 0x20 || a > 0x7e) a = 0x20;
    let attrs = this.setCharAttrs(this.bufferAddress);
    let fgndColor = this.lookupColor(attrs.color);
    let bgndColor = this.defaultBgndColor;
    let highlights = this.calculateHighlights(field, attrs);
    if ((field.attribute & this.FA_DISPLAY_MASK) !== this.FA_NONDISPLAY) {
      this.drawChar(String.fromCharCode(a), this.bufferAddress, fgndColor, bgndColor, highlights);
    }
    else {
      this.drawChar(" ", this.bufferAddress, fgndColor, bgndColor, highlights);
    }
    this.buffer[this.bufferAddress] = e;
    this.bufferAddress = (this.bufferAddress + 1) % this.buffer.length;
  }

  echoChar(c) {
    let field = this.getFieldForAddress(this.cursorAddress);
    let attrs = this.charAttrs[this.cursorAddress];
    let fgndColor = this.lookupColor(attrs.color);
    let bgndColor = this.defaultBgndColor;
    let highlights = this.calculateHighlights(field, attrs);
    if ((field.attribute & this.FA_DISPLAY_MASK) !== this.FA_NONDISPLAY) {
      this.drawChar(String.fromCharCode(c), this.cursorAddress, fgndColor, bgndColor, highlights);
    }
    else {
      this.drawChar(" ", this.cursorAddress, fgndColor, bgndColor, highlights);
    }
  }

  eraseChar() {
    this.hideCursor();
    let field = this.getFieldForAddress(this.cursorAddress);
    if (this.cursorAddress <= field.address + 1) {
      let addr = (field.address + field.size) % this.buffer.length;
      if (this.buffer[addr] !== 0) {
        this.cursorAddress = addr;
      }
      else {
        this.showCursor();
        return;
      }
    }
    this.cursorAddress -= 1;
    if (this.cursorAddress < 0) this.cursorAddress = this.buffer.length - 1;
    this.buffer[this.cursorAddress] = 0;
    let row = Math.floor(this.cursorAddress / this.cols);
    let col = this.cursorAddress % this.cols;
    this.context.fillStyle = this.defaultBgndColor;
    let y = row * this.fontHeight;
    let x = col * this.fontWidth;
    this.context.clearRect(x, y, this.fontWidth, this.fontHeight);
    this.context.fillRect(x, y, this.fontWidth, this.fontHeight);
    this.showCursor();
    this.notifyTerminalStatus();
  }

  moveCursor(incr) {
    this.cursorAddress += incr;
    if (this.cursorAddress >= this.buffer.length) {
      this.cursorAddress %= limit;
    }
    else if (this.cursorAddress < 0) {
      this.cursorAddress += this.buffer.length;
    }
    this.notifyTerminalStatus();
  }

  skipToNextField() {
    if (this.fieldAddresses.length > 0) {
      let field = this.getFieldForAddress(this.cursorAddress);
      let cursorFieldIndex = this.fieldAddresses.indexOf(field.address);
      let fieldIndex = cursorFieldIndex;
      do {
        fieldIndex = (fieldIndex + 1) % this.fieldAddresses.length;
        let address = this.fieldAddresses[fieldIndex];
        if ((this.buffer[address] & this.FA_PROTECTED) === 0) {
          field = this.getFieldForAddress(address);
          if (field.size > 0) break;
        }
      } while (fieldIndex != cursorFieldIndex);
      this.cursorAddress = (this.fieldAddresses[fieldIndex] + 1) % this.buffer.length;
      this.notifyTerminalStatus();
    }
  }

  to12Bit(address) {
    return [this.addressXlation[address >> 6], this.addressXlation[address & 0x3f]];
  }

  sendAID(aid) {
    let data = [aid].concat(this.to12Bit(this.cursorAddress), this.IAC, this.EOR);
    this.keyboardInputHandler(Uint8Array.from(data).buffer);
    this.isTerminalWait = true;
    this.notifyTerminalStatus();
    this.debug("SENT", data);
    this.debug(`....AID 0x${aid.toString(16)}`);
    this.debug(`........cursor ${this.cursorAddress} ${this.getRowColStr(this.cursorAddress)}`);
  }

  sendAIDWithData(aid, data) {
    let awd = [aid].concat(this.to12Bit(this.cursorAddress), data, this.IAC, this.EOR);
    this.keyboardInputHandler(Uint8Array.from(awd).buffer);
    this.isTerminalWait = true;
    this.notifyTerminalStatus();
    this.debug("SENT", awd);
    this.debug(`....AID 0x${aid.toString(16)}`);
    this.debug(`........cursor ${this.cursorAddress} ${this.getRowColStr(this.cursorAddress)}`);
    this.debug(`........  data ${this.toHex(data).join(" ")}`);
  }

  sendModifiedFields(aid) {
    let data = [aid].concat(this.to12Bit(this.cursorAddress));
    let fields = [];
    for (let i = 0; i < this.fieldAddresses.length; i++) {
      let field = this.getFieldForAddress(this.fieldAddresses[i]);
      if ((field.attribute & this.FA_MDT) !== 0) {
        fields.push(field);
        let bufferAddress = (field.address + 1) % this.buffer.length;
        data = data.concat(this.ORDER_SBA, this.to12Bit(bufferAddress));
        let lastNB = data.length;
        for (let j = 0; j < field.size; j++) {
          let b = this.buffer[bufferAddress];
          bufferAddress = (bufferAddress + 1) % this.buffer.length;
          if (b !== 0) {
            if (b === this.IAC) data.push(this,IAC);
            data.push(b);
            lastNB = data.length;
          }
          else {
            data.push(0x40);
          }
        }
        data.splice(lastNB);
      }
    }
    data.push(this.IAC, this.EOR);
    if (this.isDebug) {
      this.debug("SENT", data);
      this.debug(`....AID 0x${aid.toString(16)}`);
      this.debug(`........cursor ${this.cursorAddress} ${this.getRowColStr(this.cursorAddress)}`);
      while (fields.length > 0) {
        let f = fields.shift();
        let bufferAddress = (f.address + 1) % this.buffer.length;
        this.debug(`........SBA ${bufferAddress} ${this.getRowColStr(bufferAddress)} size=${f.size - 1} ${this.getFieldAttrStr(f.attribute)}`);
        let s = "";
        for (let j = 0; j < f.size; j++) {
          let b = this.buffer[bufferAddress];
          let a = this.ebcdicToAscii[b];
          bufferAddress = (bufferAddress + 1) % this.buffer.length;
          if (a >= 0x20 && a < 0x7f) {
            s += String.fromCharCode(a);
          }
          else if (b === 0) {
            s += " ";
          }
          else if (b < 0x0f) {
            s += `<0${b.toString(16)}>`
          }
          else {
            s += `<${b.toString(16)}>`
          }
        }
        this.debug(`........${s}`);
      }
    }
    this.keyboardInputHandler(Uint8Array.from(data).buffer);
    this.isTerminalWait = true;
    this.notifyTerminalStatus();
  }

  sendStructuredFields(aid, fieldData) {
    let data = [aid].concat(fieldData);
    data.push(this.IAC, this.EOR);
    this.keyboardInputHandler(Uint8Array.from(data).buffer);
    this.isTerminalWait = true;
    this.notifyTerminalStatus();
    if (this.isDebug) {
      this.debug("SENT", data);
      this.debug(`....AID 0x${aid.toString(16)}`);
      let i = 0;
      while (i < fieldData.length) {
        let len = (fieldData[i] << 8) | fieldData[i + 1];
        let id = fieldData[i + 2];
        this.debug(`........Field len=${len} id=0x${id.toString(16)}`, fieldData.slice(i + 3, i + len));
        i += len;
      }
    }
  }

  showCursor() {
    let row = Math.floor(this.cursorAddress / this.cols);
    let col = this.cursorAddress % this.cols;
    if (row < this.rows && col < this.cols) {
      this.cursorX = col * this.fontWidth;
      this.cursorY = row * this.fontHeight;
      this.cursorCharBlock = this.context.getImageData(this.cursorX, this.cursorY, this.fontWidth, this.fontHeight);
      this.context.fillStyle = this.defaultFgndColor;
      this.context.fillRect(this.cursorX, this.cursorY, this.fontWidth, this.fontHeight);
    }
    else if (this.cursorCharBlock) {
      delete this.cursorCharBlock;
    }
  }

  hideCursor() {
    if (this.cursorCharBlock) {
      this.context.putImageData(this.cursorCharBlock, this.cursorX, this.cursorY);
      delete this.cursorCharBlock;
    }
  }

  renderText(data) {

    if (typeof data === "string") {
      let ab = new Uint8Array(data.length);
      for (let i = 0; i < data.length; i++) {
        ab[i] = data.charCodeAt(i) & 0xff;
      }
      data = ab;
    }

    this.debug("RCVD", data);
    this.hideCursor();

    for (let i = 0; i < data.byteLength; i++) {
      let b = data[i];
      switch (this.state) {
      case this.ST_DATA:
        if (b === this.IAC) {
          this.state = this.ST_TN_IAC;
        }
        else {
          this.command.push(b);
          if (b === 0x0d) this.state = this.ST_TN_CR;
        }
        break;
      case this.ST_TN_CR:
        if (b !== 0) {
          this.command.push(b);
        }
        this.state = this.ST_DATA;
        break;
      case this.ST_TN_IAC:
        switch (b) {
        case this.EOR:
          this.processCommand(this.command);
          this.command = [];
          this.state = this.ST_DATA;
          break;
        case this.SE:
        case this.NOP:
        case this.DATA_MARK:
        case this.GA:
        case this.AYT:
        case this.AO:
        case this.IP:
        case this.EC:
        case this.EL:
        case this.BREAK:
          // do nothing
          this.state = this.ST_DATA;
          break;
        case this.SB:
          this.subnegotiation = [];
          this.state = this.ST_TN_SB;
          break;
        case this.WILL:
          this.state = this.ST_TN_WILL;
          break;
        case this.WONT:
          this.state = this.ST_TN_WONT;
          break;
        case this.DO:
          this.state = this.ST_TN_DO;
          break;
        case this.DONT:
          this.state = this.ST_TN_DONT;
          break;
        case this.IAC:
          this.command.push(this.IAC);
          this.state = this.ST_DATA;
          break;
        default:
          this.command.push(this.IAC, b);
          this.state = this.ST_DATA;
          break;
        }
        break;
      case this.ST_TN_WILL:
        this.debug(`RCVD: WILL ${b}`);
        switch (b) {
        case this.OPT_BINARY:
        case this.OPT_SGA:
        case this.OPT_EOR:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.DO, b]).buffer);
          this.debug(`SENT:   DO ${b}`);
          break;
        case OPT_ECHO:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.DO, b]).buffer);
          this.debug(`SENT:   DO ${b}`);
          this.doEcho = false;
          break;
        default:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.DONT, b]).buffer);
          this.debug(`SENT: DONT ${b}`);
          break;
        }
        this.state = this.ST_DATA;
        break;
      case this.ST_TN_WONT:
        this.debug(`RCVD: WONT ${b}`);
        if (b === this.OPT_ECHO) {
          this.doEcho = true;
        }
        this.keyboardInputHandler(Uint8Array.from([this.IAC, this.DONT, b]).buffer);
        this.debug(`SENT: DONT ${b}`);
        this.state = this.ST_DATA;
        break;
      case this.ST_TN_DO:
        this.debug(`RCVD:   DO ${b}`);
        switch (b) {
        case this.OPT_BINARY:
        case this.OPT_SGA:
        case this.OPT_TERM_TYPE:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.WILL, b]).buffer);
          this.debug(`SENT: WILL ${b}`);
          break;
        case this.OPT_EOR:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.WILL, b]).buffer);
          this.debug(`SENT: WILL ${b}`);
          this.doEor = true;
          break;
        case OPT_ECHO:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.WILL, b]).buffer);
          this.debug(`SENT: WILL ${b}`);
          this.doEcho = true;
          break;
        default:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.WONT, b]).buffer);
          this.debug(`SENT: WONT ${b}`);
          break;
        }
        this.state = this.ST_DATA;
        break;
      case this.ST_TN_DONT:
        this.debug(`RCVD: DONT ${b}`);
        switch (b) {
        case this.OPT_EOR:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.WONT, b]).buffer);
          this.doEor = false;
          break;
        case OPT_ECHO:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.WONT, b]).buffer);
          this.doEcho = false;
          break;
        default:
          this.keyboardInputHandler(Uint8Array.from([this.IAC, this.WONT, b]).buffer);
          break;
        }
        this.debug(`SENT: WONT ${b}`);
        this.state = this.ST_DATA;
        break;
      case this.ST_TN_SB:
        if (b === this.IAC) {
          this.state = this.ST_TN_SB_IAC;
        }
        else {
          this.subnegotiation.push(b);
        }
        break;
      case this.ST_TN_SB_IAC:
        if (b === this.SE) {
          if (this.subnegotiation[0] === this.OPT_TERM_TYPE && this.subnegotiation[1] === this.TERM_TYPE_SEND) {
            this.debug("RCVD: SB TERM_TYPE SEND");
            let bytes = [this.IAC, this.SB, this.OPT_TERM_TYPE, this.TERM_TYPE_IS];
            for (let i = 0; i < this.termType.length; i++) {
              bytes.push(this.termType.charCodeAt(i));
            }
            bytes.push(this.IAC);
            bytes.push(this.SE);
            this.keyboardInputHandler(Uint8Array.from(bytes).buffer);
            this.debug(`SENT: SB TERM_TYPE IS ${this.termType}`);
          }
          this.state = this.ST_DATA;
        }
        else if (b === this.IAC) {
          this.subnegotiation.push(b);
          this.state = this.ST_TN_SB;
        }
        else {
          this.subnegotiation.push(this.IAC);
          this.subnegotiation.push(b);
          this.state = this.ST_TN_SB;
        }
        break;
      }
    }

    this.showCursor();
  }

  calculateBufferAddress(b1, b2) {
    let addr = (b1 << 8) | b2;
    if (!this.is16BitMode) {
      if ((addr & 0xc000) === 0) { // 14-bit mode
        addr &= 0x3fff;
      }
      else { // 12-bit mode
        addr = ((addr & 0x3f00) >> 2) | (addr & 0x3f);
      }
    }
    return addr;
  }

  getFieldForAddress(addr) {
    if (this.fieldAddresses.length < 1) {
      return {address:0, size:this.buffer.length, attribute:this.FA_PROTECTED};
    }
    else if (addr < this.fieldAddresses[0]) {
      let fieldAddress = this.fieldAddresses[this.fieldAddresses.length - 1];
      let size = ((this.buffer.length - fieldAddress) - 1) + this.fieldAddresses[0];
      return {address:fieldAddress, size:size, attribute:this.buffer[fieldAddress]};
    }
    let i = 0;
    let fieldAddress = this.fieldAddresses[0];
    let size = this.buffer.length - 1;
    while (i < this.fieldAddresses.length) {
      if (this.fieldAddresses[i] > addr) {
        size = (this.fieldAddresses[i] - fieldAddress) - 1;
        return {address:fieldAddress, size:size, attribute:this.buffer[fieldAddress]};
      }
      fieldAddress = this.fieldAddresses[i];
      i += 1;
    }
    fieldAddress = this.fieldAddresses[i - 1];
    size = ((this.buffer.length - fieldAddress) - 1) + this.fieldAddresses[0];
    return {address:fieldAddress, size:size, attribute:this.buffer[fieldAddress]};
  }

  getFieldAttrStr(fieldAttr) {
    let attrNames = [];
    if ((fieldAttr & this.FA_PROTECTED   ) !== 0                  ) attrNames.push("Protected");
    if ((fieldAttr & this.FA_NUMERIC     ) !== 0                  ) attrNames.push("Numeric");
    if ((fieldAttr & this.FA_DISPLAY_MASK) === this.FA_DISPLAY_PEN) attrNames.push("Display");
    if ((fieldAttr & this.FA_DISPLAY_MASK) === this.FA_INTENSE    ) attrNames.push("Intensified");
    if ((fieldAttr & this.FA_DISPLAY_MASK) === this.FA_NONDISPLAY ) attrNames.push("Non-Display");
    if ((fieldAttr & this.FA_MDT         ) !== 0                  ) attrNames.push("Modified");
    return attrNames.join(",");
  }

  getRowColStr(addr) {
    return `(${Math.floor(addr / this.cols)},${addr % this.cols})`;
  }

  processWrite(wcc, data) {
    if ((wcc & this.WCC_RESET_MDT) !== 0) {
      for (let i = 0; i < this.fieldAddresses.length; i++) {
        this.buffer[this.fieldAddresses[i]] &= ~this.FA_MDT;
      }
    }
    let i = 0;
    let charsWritten = "";
    while (i < data.length) {
      let b = data[i++];
      if (b < 0x40) { // it's an order code
        if (this.isDebug && charsWritten.length > 0) {
          this.debug(`........${charsWritten}`);
          charsWritten = "";
        }
        switch (b) {
        case this.ORDER_SF:  // Start Field
          if (i < data.length) {
            let fieldAttr = data[i++];
            let field = this.getFieldForAddress(this.bufferAddress);
            let currentFieldAttr = field.attribute;
            if (this.fieldAddresses.indexOf(this.bufferAddress) === -1) {
              this.fieldAddresses.push(this.bufferAddress);
              this.fieldAddresses.sort((a, b) => a - b);
            }
            // Field attributes don't have any character attributes
            this.drawChar(" ", this.bufferAddress, this.defaultFgndColor, this.defaultBgndColor, 0);
            this.buffer[this.bufferAddress] = fieldAttr;
            if (this.isDebug) {
              this.debug(`........SF ${this.bufferAddress} ${this.getRowColStr(this.bufferAddress)} : ${this.getFieldAttrStr(fieldAttr)}`);
              if (this.debugFields) {
                for (let i = 0; i < this.fieldAddresses.length; i++) {
                  let f = this.getFieldForAddress(this.fieldAddresses[i]);
                  this.debug(`........   field[${i}]: ${f.address} ${this.getRowColStr(f.address)} size=${f.size} ${this.getFieldAttrStr(f.attribute)}`);
                }
              }
            }
            this.bufferAddress = (this.bufferAddress + 1) % this.buffer.length;
            //
            // If the display attribute value is changing, rewrite all characters of the
            // new/updated field to reflect the new value.
            //
            if (((fieldAttr ^ currentFieldAttr) & this.FA_DISPLAY_MASK) !== 0) {
              let bufferAddress = this.bufferAddress;
              field = this.getFieldForAddress(bufferAddress);
              let charAttrs = this.charAttrs[bufferAddress];
              let fgndColor = this.lookupColor(charAttrs.color);
              let bgndColor = this.defaultBgndColor;
              let highlights = this.calculateHighlights(field, charAttrs);
              this.debug(`........   display attribute value change, redraw ${field.size} chars`);
              for (let i = 0; i < field.size; i++) {
                let a = this.ebcdicToAscii[this.buffer[bufferAddress]];
                if (a < 0x20 || a > 0x7e) a = 0x20;
                if ((fieldAttr & this.FA_DISPLAY_MASK) !== this.FA_NONDISPLAY) {
                  this.drawChar(String.fromCharCode(a), bufferAddress, fgndColor, bgndColor, highlights);
                }
                else {
                  this.drawChar(" ", bufferAddress, fgndColor, bgndColor, highlights);
                }
                bufferAddress = (bufferAddress + 1) % this.buffer.length;
              }
            }
          }
          break;
        case this.ORDER_SFE: // Start Field Extended
          if (i < data.length && (i + data[i] * 2) < data.length) {
            let n = data[i++];
            let attrs = [];
            while (n > 0) {
              attrs.push[{type:data[i], value:data[i + 1]}];
              i += 2;
              n -= 1;
            }
            // TODO: finish this
            if (this.isDebug) {
              let pairs = [];
              for (let pair of attrs) {
                pairs.push(`<${pair.type.toString(16)}>=<${pair.value.toString(16)}>`);
              }
              this.debug(`........SFE ${this.bufferAddress} ${this.getRowColStr(this.bufferAddress)} : ${pairs.join(",")}`);
            }
            else console.log("ORDER_SFE received");
          }
          break;
        case this.ORDER_SBA: // Set Buffer Address
          if (i + 1 < data.length) {
            let addr = this.calculateBufferAddress(data[i], data[i + 1]);
            i += 2;
            if (addr < this.buffer.length) {
              this.bufferAddress = addr;
              if (this.isDebug) {
                let f = this.getFieldForAddress(addr);
                this.debug(`........SBA ${addr} ${this.getRowColStr(addr)} field:[${this.getRowColStr(f.address)} size=${f.size} ${this.getFieldAttrStr(f.attribute)}]`);
              }
            }
            else {
              this.debug(`........SBA invalid address ${addr}`);
            }
          }
          break;
        case this.ORDER_SA:  // Set Attribute
          if (i + 1 < data.length) {
            let attrType = data[i++];
            let attrVal  = data[i++];
            this.debug(`........SA ${this.bufferAddress} ${this.getRowColStr(this.bufferAddress)} : <${attrType.toString(16)}>=<${attrVal.toString(16)}>`);
            switch (attrType) {
            case 0x00: // all character attributes
              this.resetCurrentCharAttrs();
              break;
            case 0x41: // extended highlighting
              this.currentCharAttrs.highlight = attrVal;
              break;
            case 0x42: // foreground color
              this.currentCharAttrs.color = attrVal;
              break;
            default:
              console.log(`Unsupported attribute type 0x${attrType.toString(16)} received in SA order`);
              break;
            }
          }
          break;
        case this.ORDER_MF:  // Modify Field
          if (i < data.length && (i + data[i] * 2) < data.length) {
            let n = data[i++];
            let attrs = [];
            while (n > 0) {
              attrs.push[{type:data[i], value:data[i + 1]}];
              i += 2;
              n -= 1;
            }
            // TODO: finish this
            if (this.isDebug) {
              let pairs = [];
              for (let pair of attrs) {
                pairs.push(`<${pair.type.toString(16)}>=<${pair.value.toString(16)}>`);
              }
              this.debug(`........MF ${this.bufferAddress} ${this.getRowColStr(this.bufferAddress)} : ${pairs.join(",")}`);
            }
            else console.log("ORDER_MF received");
          }
          break;
        case this.ORDER_IC:  // Insert Cursor
          this.hideCursor();
          this.cursorAddress = this.bufferAddress;
          this.showCursor();
          if (this.isDebug) {
            this.debug(`........IC ${this.cursorAddress} ${this.getRowColStr(this.cursorAddress)}`);
          }
          break;
        case this.ORDER_PT:  // Program Tab
          // TODO: if this order doesn't immediately follow a command or order,
          //       e.g., if it follows a wcc, then the remainder of the field is cleared to NUL's
          this.debug("........PT ${this.bufferAddress} ${this.getRowColStr(this.bufferAddress)}");
          if (this.fieldAddresses.length > 0) {
            let field = this.getFieldForAddress(this.bufferAddress);
            if (field.address === this.bufferAddress && (this.buffer[field.address] & this.FA_PROTECTED) === 0) {
              this.bufferAddress = (this.bufferAddress + 1) % this.buffer.length;
            }
            else {
              let startFieldIndex = this.fieldAddresses.indexOf(field.address);
              let fieldIndex = startFieldIndex;
              do {
                fieldIndex = (fieldIndex + 1) % this.fieldAddresses.length;
                let address = this.fieldAddresses[fieldIndex];
                if ((this.buffer[address] & this.FA_PROTECTED) === 0) {
                  field = this.getFieldForAddress(address);
                  if (field.size > 0) break;
                }
              } while (fieldIndex != startFieldIndex);
              this.bufferAddress = (this.fieldAddresses[fieldIndex] + 1) % this.buffer.length;
            }
          }
          break;
        case this.ORDER_RA:  // Repeat to Address
          if (i + 2 < data.length) {
            let addr = this.calculateBufferAddress(data[i], data[i + 1]);
            i += 2;
            let ch = data[i++];
            this.debug(`........RA ${addr} ${this.getRowColStr(addr)} <${ch.toString(16)}>`);
            if (addr < this.buffer.length) {
              do {
                this.writeChar(ch);
              }
              while (this.bufferAddress !== addr);
            }
          }
          break;
        case this.ORDER_EUA: // Erase Unprotected to Address
          if (i + 1 < data.length) {
            let addr = this.calculateBufferAddress(data[i], data[i + 1]);
            i += 2;
            if (addr < this.buffer.length) {
              let n = 0;
              do {
                let field = this.getFieldForAddress(this.bufferAddress);
                let attrs = this.resetCharAttrs(this.bufferAddress);
                let fgndColor = this.lookupColor(attrs.color);
                let bgndColor = this.defaultBgndColor;
                if ((field.attribute & this.FA_PROTECTED) === 0 && field.address !== this.bufferAddress) {
                  this.drawChar(" ", this.bufferAddress, fgndColor, bgndColor, 0);
                  this.buffer[this.bufferAddress] = 0;
                  n += 1;
                }
                this.bufferAddress = (this.bufferAddress + 1) % this.buffer.length;
              }
              while (this.bufferAddress !== addr);
              if (this.isDebug) {
                this.debug(`........EUA ${addr} ${this.getRowColStr(addr)} erase ${n} chars`);
              }
            }
            else {
              this.debug(`........EUA invalid address ${addr}`);
            }
          }
          break;
        case this.ORDER_GE:  // Graphic Escape
          // TODO: implement this
          if (i < data.length) {
            let charCode = data[i++];
            this.debug(`........GE ${this.bufferAddress} ${this.getRowColStr(this.bufferAddress)} : 0x${charCode.toString(16)}`);
            if (!this.isDebug)
              console.log(`........GE ${this.bufferAddress} ${this.getRowColStr(this.bufferAddress)} : 0x${charCode.toString(16)}`);
          }
          break;
        case this.ORDER_NUL: // Blank, suppressed on Read Modified
        case this.ORDER_FF:  // Blank
        case this.ORDER_CR:  // Blank
        case this.ORDER_NL:  // Blank
        case this.ORDER_EM:  // Blank
        case this.ORDER_EO:  // Blank
          this.writeChar(0x40);
          this.buffer[(this.bufferAddress > 0) ? this.bufferAddress - 1 : this.buffer.length - 1] = b;
          if (this.isDebug) charsWritten += " ";
          break;
        // TODO: draw glyphs for the following (are DUP and FM overstrikes?)
        case this.ORDER_SUB: // Solid circle
        case this.ORDER_DUP: // Overscore asterisk
        case this.ORDER_FM:  // Overscore semicolon
          this.writeChar(0x40);
          this.buffer[(this.bufferAddress > 0) ? this.bufferAddress - 1 : this.buffer.length - 1] = b;
          if (this.isDebug) charsWritten += " ";
          break;
        default: // Invalid order code
          this.debug(`........Unknown order ${b}`);
          break;
        }
      }
      else {
        this.writeChar(b);
        if (this.isDebug) {
          let cc = this.ebcdicToAscii[b];
          charsWritten += (cc >= 0x20 && cc < 0x7f)
            ? String.fromCharCode(cc)
            : `<${b.toString(16)}>`;
        }
      }
    }
    if (this.isDebug && charsWritten.length > 0) this.debug(`........${charsWritten}`);
    if ((wcc & this.WCC_KB_RESTORE) !== 0) {
      this.isTerminalWait = false;
    }
    this.notifyTerminalStatus();
  }

  processW(wcc, data) {
    this.debugCmd("W", wcc);
    this.bufferAddress = this.cursorAddress;
    this.processWrite(wcc, data);
  }

  processEW(wcc, data) {
    this.debugCmd("EW", wcc);
    this.reset();
    this.processWrite(wcc, data);
  }

  processEWA(wcc, data) {
    this.debugCmd("EWA", wcc);
    this.reset();
    this.processWrite(wcc, data);
  }

  processEAU(data) {
    this.debugCmd("EAU");
    this.hideCursor();
    //
    // Clear all unprotected characters
    //
    for (let addr = 0; addr < this.buffer.length; addr++) {
      let field = this.getFieldForAddress(addr);
      let attrs = this.resetCharAttrs(addr);
      let fgndColor = this.lookupColor(attrs.color);
      let bgndColor = this.defaultBgndColor;
      if ((field.attribute & this.FA_PROTECTED) === 0 && field.address !== addr) {
        this.drawChar(" ", addr, fgndColor, bgndColor, 0);
        this.buffer[addr] = 0;
      }
    }
    //
    // Clear MDT bits in all unprotected fields
    //
    let firstUnprotectedField = null;
    for (let i = 0; i < this.fieldAddresses.length; i++) {
      if ((this.buffer[this.fieldAddresses[i]] & this.FA_PROTECTED) === 0) {
        if (firstUnprotectedField === null) firstUnprotectedField = this.fieldAddresses[i];
        this.buffer[this.fieldAddresses[i]] &= ~FA_MDT;
      }
    }
    this.cursorAddress = (firstUnprotectedField !== null) ? (firstUnprotectedField + 1) % this.buffer.length : 0;
    this.showCursor();
    this.isTerminalWait = false;
    this.notifyTerminalStatus();
  }

  processRB(data) {
    this.debugCmd("RB");
    let response = [this.cursorAddress >> 8, this.cursorAddress & 0xff];
    for (let address = 0; address < this.buffer.length; address++) {
      if (this.fieldAddresses.indexOf(address) >= 0) response.push(this.ORDER_SF);
      let b = this.buffer[address];
      if (b === this.IAC) response.push(this.IAC);
      response.push(b);
    }
    this.sendStructuredFields(this.AID_SF, response);
  }

  readModifiedFields() {
    let response = [this.cursorAddress >> 8, this.cursorAddress & 0xff];
    if (this.fieldAddresses.length > 0) {
      for (let i = 0; i < this.fieldAddresses.length; i++) {
        let address = this.fieldAddresses[i];
        if ((this.buffer[address] & this.FA_MDT) !== 0) {
          let field = this.getFieldForAddress(address);
          response.push(this.ORDER_SF);
          let b = this.buffer[address++];
          if (b === this.IAC) response.push(this.IAC);
          response.push(b);
          while (field.size-- > 0) {
            b = this.buffer[address++];
            if (b === this.IAC) {
              response.push(this.IAC, b);
            }
            else if (b !== 0) {
              response.push(b);
            }
          }
        }
      }
    }
    else { // the buffer is unstructured (no fields are defined)
      for (let b of this.buffer) {
        if (b === this.IAC) {
          response.push(this.IAC, b);
        }
        else if (b !== 0) {
          response.push(b);
        }
      }
      this.bufferAddress = 0;
    }
    this.sendStructuredFields(this.AID_SF, response);
  }

  processRM(data) {
    this.debugCmd("RM");
    this.readModifiedFields();
  }

  processRMA(data) {
    this.debugCmd("RMA");
    this.readModifiedFields();
  }

  processSF_ReadPartition(data) {
    this.debug("........Read Partition", data);
    if (data.length > 1) {
      let pid = data[0];
      let type = data[1];
      let qcs = [this.QR_USABLE_AREA, this.QR_CHARACTER_SETS, this.QR_IMPLICIT_PARTITION,
                 this.QR_ALPHANUMERIC_PARTITIONS, this.QR_COLOR, this.QR_HIGHLIGHT,
                 this.QR_REPLY_MODES, this.QR_RPQ_NAMES];
      if (type === this.RPT_QUERY_LIST) {
        if (data.length > 2) {
          let reqType = data[2] & 0xc0;
          if (reqType === 0) {
            if (data.length > 3) {
              qcs = [];
              for (let i = 3; i < data.length; i++) {
                switch (data[i]) {
                case this.QR_USABLE_AREA:
                case this.QR_CHARACTER_SETS:
                case this.QR_IMPLICIT_PARTITION:
                case this.QR_ALPHANUMERIC_PARTITIONS:
                case this.QR_COLOR:
                case this.QR_HIGHLIGHT:
                case this.QR_REPLY_MODES:
                case this.QR_RPQ_NAMES:
                  if (qcs.indexOf(data[i]) === -1) qcs.push(data[i]);
                  break;
                default:
                  // discard unsupported QR code
                  break;
                }
              }
              if (qcs.length < 1) qcs = [this.QR_NULL];
            }
            else {
              qcs = [this.QR_NULL];
            }
          }
          else if (reqType === 0xc0) {
            console.log(`Invalid Read Partition Query List request type 0x${data[2].toString(16)}`);
            return;
          }
        }
        else {
          console.log(`Invalid Read Partition length ${data.length} in Query List`);
          return;
        }
      }
      else if (type !== this.RPT_QUERY) {
        console.log(`Unsupported Read Partition type 0x${type.toString(16)}`);
        return;
      }
      qcs.unshift(this.QR_SUMMARY);
      let response = [];
      for (let i = 0; i < qcs.length; i++) {
        let queryReply = [];
        let len = 0;
        switch (qcs[i]) {
        case this.QR_USABLE_AREA:
          queryReply = [
            this.QR_USABLE_AREA,
            0x03, // Flags (12/14/16-bit addressing supported)
               0, // Flags
            0,80, // Width  in cells
            0,43, // Height in cells
               1, // Units (millimeters)
            0x00, // Xr (numerator)
            0x0a, // Xr (numerator)
            0x02, // Xr (denominator)
            0xe5, // Xr (denominator)
            0x00, // Yr (numerator)
            0x02, // Yr (numerator)
            0x00, // Yr (denominator)
            0x6f, // Yr (denominator)
               9, // AW
              12, // AH
            0x0d, // BUFSIZ
            0x70  // BUFSIZ
          ];
          len = queryReply.length;
          break;
        case this.QR_CHARACTER_SETS:
          queryReply = [
            this.QR_CHARACTER_SETS,
            0x02, // Flags
               0, // Flags
               0, // SDW
               0, // SDH
               0, // FORM
               0, // FORM
               0, // FORM
               0, // FORM
               7, // DL
               0, // SET
            0x10, // Flags
               0, // LCID
            0x02, // CGCSGID
            0xb9, // CGCSGID
            0x00, // CGCSGID
            0x25  // CGCSGID
          ];
          len = queryReply.length;
          break;
        case this.QR_IMPLICIT_PARTITION:
          queryReply = [
            this.QR_IMPLICIT_PARTITION,
               0, // Reserved
               0, // Reserved
            0x0b, // Length
            0x01, // Implicit partition sizes
            0x00, // Reserved
            0,80, // Width of default screen size
            0,43, // Height of default screen size
            0,80, // Width of alternate screen size
            0,43  // Height of alternate screen size
          ];
          len = queryReply.length;
          break;
        case this.QR_ALPHANUMERIC_PARTITIONS:
          queryReply = [
            this.QR_ALPHANUMERIC_PARTITIONS,
               0, // Maximum number of alphanuermic partitions
            this.buffer.length >> 8, // Total available partition storage
            this.buffer.length & 0xff,
            0x00 // Flags
          ];
          len = queryReply.length;
          break;
        case this.QR_COLOR:
          queryReply = [
            this.QR_COLOR,
               0, // Flags
              16, // NP
            0x00, // CAV(0)
            0xf4, // CI (0)
            0xf1, // CAV(1)
            0xf1, // CI (1)
            0xf2, // CAV(2)
            0xf2, // CI (2)
            0xf3, // CAV(3)
            0xf3, // CI (3)
            0xf4, // CAV(4)
            0xf4, // CI (4)
            0xf5, // CAV(5)
            0xf5, // CI (5)
            0xf6, // CAV(6)
            0xf6, // CI (6)
            0xf7, // CAV(7)
            0xf7, // CI (7)
            0xf8, // CAV(8)
            0xf8, // CI (8)
            0xf9, // CAV(9)
            0xf9, // CI (9)
            0xfa, // CAV(10)
            0xfa, // CI (10)
            0xfb, // CAV(11)
            0xfb, // CI (11)
            0xfc, // CAV(12)
            0xfc, // CI (12)
            0xfd, // CAV(13)
            0xfd, // CI (13)
            0xfe, // CAV(14)
            0xfe, // CI (14)
            this.IAC,
            0xff, // CAV(15)
            this.IAC,
            0xff  // CI (15)
          ];
          len = queryReply.length - 2; // deduct the two IAC's
          break;
        case this.QR_HIGHLIGHT:
          queryReply = [
            this.QR_HIGHLIGHT,
               4, // NP
            0x00, // V(0)
            0xf0, // A(0) Normal highlight
            0xf2, // V(1)
            0xf2, // A(1) Reverse video
            0xf4, // V(2)
            0xf4, // A(2) Underscore
            0xf8, // V(3)
            0xf8  // A(3) Intensify
          ];
          len = queryReply.length;
          break;
        case this.QR_REPLY_MODES:
          queryReply = [
            this.QR_REPLY_MODES,
            this.RM_FIELD,
            this.RM_EXTENDED_FIELD,
            this.RM_CHARACTER
          ];
          len = queryReply.length;
          break;
        case this.QR_RPQ_NAMES:
          queryReply = [
            this.QR_RPQ_NAMES,
            0x00,0x00, // Device type identifier
            0x00,0x00, // Model type identifier
               8,      // RPQ length
            0xa6,      // 'W'
            0xa6,      // 'W'
            0xa6,      // 'W'
            0xf3,      // '3'
            0xf2,      // '2'
            0xf7,      // '7'
            0xf0       // '0'
          ];
          len = queryReply.length;
          break;
        case this.QR_SUMMARY:
          queryReply = [
            this.QR_SUMMARY, this.QR_SUMMARY, this.QR_USABLE_AREA, this.QR_CHARACTER_SETS, this.QR_IMPLICIT_PARTITION,
            this.QR_ALPHANUMERIC_PARTITIONS, this.QR_COLOR, this.QR_HIGHLIGHT, this.QR_REPLY_MODES, this.QR_RPQ_NAMES
          ];
          len = queryReply.length;
          break;
        case this.QR_NULL:
          queryReply = [this.QR_NULL];
          len = queryReply.length;
          break;
        default: // should never arrive here
          console.log(`Unexpected Query Reply code 0x${qcs[i].toString(16)}`);
          continue;
        }
        len += 3;
        response.push(len >> 8, len & 0xff, this.SFC_QUERY_REPLY);
        response = response.concat(queryReply);
      }
      this.sendStructuredFields(this.AID_SF, response);
    }
  }

  processSF_EraseRest(data) {
    // TODO: implement this
    this.debug("........Erase/Reset", data);
    if (!this.isDebug) console.log("SF Erase/Reset received");
  }

  processSF_Outbound3270DS(data) {
    // TODO: implement this
    this.debug("........Outbound 3270DS", data);
    if (!this.isDebug) console.log("SF Outbound 3270DS received");
  }

  processWSF(data) {
    this.debugCmd("WSF");
    let i = 0;
    while (i + 1 < data.length) {
      let len = (data[i] << 8) | data[i + 1];
      if (i + len <= data.length) {
        let fieldData = data.slice(i + 2, i + len);
        i += len;
        if (fieldData.length > 0) {
          let id = fieldData[0];
          if ((id === 0x0f || id === 0x10) && fieldData.length > 1) {
            id = (id << 8) | fieldData[1];
            fieldData = fieldData.slice(2);
          }
          else {
            fieldData = fieldData.slice(1);
          }
          switch (id) {
          case this.SFC_READ_PARTITION:
            this.processSF_ReadPartition(fieldData);
            break;
          case this.SFC_ERASE_RESET:
            this.processSF_EraseReset(fieldData);
            break;
          case this.SFC_OUTBOUND_3270DS:
            this.processSF_Outbound3270DS(fieldData);
            break;
          default:
            console.log(`Unsupported structured field with ID 0x${id.toString(16)} received in WSF order`);
            break;
          }
        }
      }
      else {
        break;
      }
    }
  }

  processCommand(command) {
    if (command.length < 1) return;
    let i = 0;
    let cmd = command[i++];
    let wcc = 0;
    switch (cmd) {
    case this.CC_W:   // Write
      if (i >= command.length) return;
      wcc = command[i++];
      this.resetCurrentCharAttrs();
      this.processW(wcc, command.slice(i));
      break;
    case this.CC_EW:  // Erase/Write
      if (i >= command.length) return;
      wcc = command[i++];
      this.resetCurrentCharAttrs();
      this.processEW(wcc, command.slice(i));
      break;
    case this.CC_EWA: // Erase/Write Alternate
      if (i >= command.length) return;
      wcc = command[i++];
      this.resetCurrentCharAttrs();
      this.processEWA(wcc, command.slice(i));
      break;
    case this.CC_RB:  // Read Buffer
      this.processRB(command.slice(i));
      break;
    case this.CC_RM:  // Read Modified
      this.processRM(command.slice(i));
      break;
    case this.CC_RMA: // Read Modified All
      this.processRMA(command.slice(i));
      break;
    case this.CC_EAU: // Erase All Unprotected
      this.resetCurrentCharAttrs();
      this.processEAU(command.slice(i));
      break;
    case this.CC_WSF: // Write Structured Field
      this.processWSF(command.slice(i));
      break;
    default:
      this.debug(`Unrecognized 3270 command: 0x${cmd.toString(16)}`);
      break;
    }
  }
}
