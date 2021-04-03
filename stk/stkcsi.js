const fs = require("fs");
const net = require("net");
const Program = require("./program");
const ProgramRegistry = require("./registry");
const RPC = require("./rpc");
const XDR = require("./xdr");

class StkCSI extends Program {

  static PROGRAM             = 0x493FF;
  static VERSION             = 1;
  static RPC_PORT            = 4400;

  static END_OF_MEDIUM       = 0xffffffff;
  static ERASE_GAP           = 0xfffffffe;
  static TAPE_MARK           = 0x00000000;
  static TAPE_READ_FAILURE   = 0x80000000;
  static RECORD_ERROR_FLAG   = 0x80000000;

  static COMMAND_AUDIT       = 1;
  static COMMAND_CANCEL      = 2;
  static COMMAND_DISMOUNT    = 3;
  static COMMAND_EJECT       = 4;
  static COMMAND_ENTER       = 5;
  static COMMAND_IDLE        = 6;
  static COMMAND_MOUNT       = 7;
  static COMMAND_QUERY       = 8;
  static COMMAND_RECOVERY    = 9;
  static COMMAND_START       = 10;
  static COMMAND_VARY        = 11;

  static LOCATION_CELL       = 1;
  static LOCATION_DRIVE      = 2;

  static OPTION_FORCE        = 0x01;
  static OPTION_INTERMEDIATE = 0x02;
  static OPTION_ACKNOWLEDGE  = 0x04;

  static QUERY_TYPE_ALL      = 1;
  static QUERY_TYPE_NEXT     = 2;
  static QUERY_TYPE_ONE      = 3;

  static SELECT_OPTION_ACS   = 1;
  static SELECT_OPTION_LSM   = 2;

  static STATE_CANCELLED     = 1;
  static STATE_DIAGNOSTIC    = 2;
  static STATE_IDLE          = 3;
  static STATE_IDLE_PENDING  = 4;
  static STATE_OFFLINE       = 5;
  static STATE_OFFLINE_PENDING = 6;
  static STATE_ONLINE        = 7;
  static STATE_RECOVERY      = 8;
  static STATE_RUN           = 9;

  static TYPE_ACS            = 1;
  static TYPE_AUDIT          = 2;
  static TYPE_CAP            = 3;
  static TYPE_CELL           = 4;
  static TYPE_CP             = 5;
  static TYPE_CSI            = 6;
  static TYPE_DISMOUNT       = 7;
  static TYPE_EJECT          = 8;
  static TYPE_EL             = 9;
  static TYPE_ENTER          = 10;
  static TYPE_DRIVE          = 11;
  static TYPE_IPC            = 12;
  static TYPE_LH             = 13;
  static TYPE_LM             = 14;
  static TYPE_LSM            = 15;
  static TYPE_MOUNT          = 16;
  static TYPE_NONE           = 17;
  static TYPE_PANEL          = 18;
  static TYPE_PORT           = 19;
  static TYPE_QUERY          = 20;
  static TYPE_RECOVERY       = 21;
  static TYPE_REQUEST        = 22;
  static TYPE_SA             = 23;
  static TYPE_SERVER         = 24;
  static TYPE_SUBPANEL       = 25;
  static TYPE_VARY           = 26;
  static TYPE_VOLUME         = 27;

  static VOLUME_TYPE_DIAGNOSTIC = 1;
  static VOLUME_TYPE_STANDARD   = 2;

  static STATUS_SUCCESS               = 0;
  static STATUS_ACS_FULL              = 1;
  static STATUS_ACS_NOT_IN_LIBRARY    = 2;
  static STATUS_ACS_OFFLINE           = 3;
  static STATUS_ACSLM_IDLE            = 4;
  static STATUS_ACTIVITY_END          = 5;
  static STATUS_ACTIVITY_START        = 6;
  static STATUS_AUDIT_ACTIVITY        = 7;
  static STATUS_AUDIT_IN_PROGRESS     = 8;
  static STATUS_CANCELLED             = 9;
  static STATUS_CAP_AVAILABLE         = 10;
  static STATUS_CAP_FULL              = 11;
  static STATUS_CAP_IN_USE            = 12;
  static STATUS_CELL_EMPTY            = 13;
  static STATUS_CELL_FULL             = 14;
  static STATUS_CELL_INACCESSIBLE     = 15;
  static STATUS_CELL_RESERVED         = 16;
  static STATUS_CLEAN_DRIVE           = 17;
  static STATUS_COMMUNICATION_FAILED  = 18;
  static STATUS_CONFIGURATION_ERROR   = 19;
  static STATUS_COUNT_TOO_SMALL       = 20;
  static STATUS_COUNT_TOO_LARGE       = 21;
  static STATUS_CURRENT               = 22;
  static STATUS_DATABASE_ERROR        = 23;
  static STATUS_DEGRADED_MODE         = 24;
  static STATUS_DONE                  = 25;
  static STATUS_DOOR_CLOSED           = 26;
  static STATUS_DOOR_OPENED           = 27;
  static STATUS_DRIVE_AVAILABLE       = 28;
  static STATUS_DRIVE_IN_USE          = 29;
  static STATUS_DRIVE_NOT_IN_LIBRARY  = 30;
  static STATUS_DRIVE_OFFLINE         = 31;
  static STATUS_DRIVE_RESERVED        = 32;
  static STATUS_DUPLICATE_LABEL       = 33;
  static STATUS_EJECT_ACTIVITY        = 34;
  static STATUS_ENTER_ACTIVITY        = 35;
  static STATUS_EVENT_LOG_FULL        = 36;
  static STATUS_IDLE_PENDING          = 37;
  static STATUS_INPUT_CARTRIDGES      = 38;
  static STATUS_INVALID_ACS           = 39;
  static STATUS_INVALID_COLUMN        = 40;
  static STATUS_INVALID_COMMAND       = 41;
  static STATUS_INVALID_DRIVE         = 42;
  static STATUS_INVALID_LSM           = 43;
  static STATUS_INVALID_MESSAGE       = 44;
  static STATUS_INVALID_OPTION        = 45;
  static STATUS_INVALID_PANEL         = 46;
  static STATUS_INVALID_PORT          = 47;
  static STATUS_INVALID_ROW           = 48;
  static STATUS_INVALID_STATE         = 49;
  static STATUS_INVALID_SUBPANEL      = 50;
  static STATUS_INVALID_TYPE          = 51;
  static STATUS_INVALID_VALUE         = 52;
  static STATUS_INVALID_VOLUME        = 53;
  static STATUS_IPC_FAILURE           = 54;
  static STATUS_LIBRARY_BUSY          = 55;
  static STATUS_LIBRARY_FAILURE       = 56;
  static STATUS_LIBRARY_NOT_AVAILABLE = 57;
  static STATUS_LOCATION_OCCUPIED     = 58;
  static STATUS_LSM_FULL              = 59;
  static STATUS_LSM_NOT_IN_LIBRARY    = 60;
  static STATUS_LSM_OFFLINE           = 61;
  static STATUS_MESSAGE_NOT_FOUND     = 62;
  static STATUS_MESSAGE_TOO_LARGE     = 63;
  static STATUS_MESSAGE_TOO_SMALL     = 64;
  static STATUS_MISPLACED_TAPE        = 65;
  static STATUS_MULTI_ACS_AUDIT       = 66;
  static STATUS_NORMAL                = 67;
  static STATUS_NONE                  = 68;
  static STATUS_NOT_IN_SAME_ACS       = 69;
  static STATUS_ONLINE                = 70;
  static STATUS_OFFLINE               = 71;
  static STATUS_PENDING               = 72;
  static STATUS_PORT_NOT_IN_LIBRARY   = 73;
  static STATUS_PROCESS_FAILURE       = 74;
  static STATUS_RECOVERY_COMPLETE     = 75;
  static STATUS_RECOVERY_FAILED       = 76;
  static STATUS_RECOVERY_INCOMPLETE   = 77;
  static STATUS_RECOVERY_STARTED      = 78;
  static STATUS_REMOVE_CARTRIDGES     = 79;
  static STATUS_RETRY                 = 80;
  static STATUS_STATE_UNCHANGED       = 81;
  static STATUS_TERMINATED            = 82;
  static STATUS_VALID                 = 83;
  static STATUS_VALUE_UNCHANGED       = 84;
  static STATUS_VARY_DISALLOWED       = 85;
  static STATUS_VOLUME_ADDED          = 86;
  static STATUS_VOLUME_EJECTED        = 87;
  static STATUS_VOLUME_ENTERED        = 88;
  static STATUS_VOLUME_FOUND          = 89;
  static STATUS_VOLUME_HOME           = 90;
  static STATUS_VOLUME_IN_DRIVE       = 91;
  static STATUS_VOLUME_IN_TRANSIT     = 92;
  static STATUS_VOLUME_NOT_IN_DRIVE   = 93;
  static STATUS_VOLUME_NOT_IN_LIBRARY = 94;
  static STATUS_UNREADABLE_LABEL      = 95;
  static STATUS_UNSUPPORTED_OPTION    = 96;
  static STATUS_UNSUPPORTED_STATE     = 97;
  static STATUS_UNSUPPORTED_TYPE      = 98;
  static STATUS_VOLUME_IN_USE         = 99;
  static STATUS_PORT_FAILURE          = 100;
  static STATUS_MAX_PORTS             = 101;
  static STATUS_PORT_ALREADY_OPEN     = 102;
  static STATUS_QUEUE_FAILURE         = 103;
  static STATUS_RPC_FAILURE           = 104;
  static STATUS_NI_TIMEDOUT           = 105;
  static STATUS_INVALID_COMM_SERVICE  = 106;
  static STATUS_COMPLETE              = 107;
  static STATUS_AUDIT_FAILED          = 108;
  static STATUS_NO_PORTS_ONLINE       = 109;
  static STATUS_CARTRIDGES_IN_CAP     = 110;
  static STATUS_TRANSLATION_FAILURE   = 111;
  static STATUS_DATABASE_DEADLOCK     = 112;
  static STATUS_DIAGNOSTIC            = 113;
  static STATUS_DUPLICATE_IDENTIFIER  = 114;
  static STATUS_EVENT_LOG_FAILURE     = 115;
  
static EXTERNAL_LABEL_SIZE            = 6;   // maximum length of volume identifier

  static MIN_ACS                      = 0;   // minimum automated cartridge server number
  static MAX_ACS                      = 127; // maximum automated cartridge server number
  static MIN_DRIVE                    = 0;   // minimum cartridge tape drive number
  static MAX_DRIVE                    = 3;   // maximum cartridge tape drive number
  static MIN_LSM                      = 0;   // minimum library storage module number
  static MAX_LSM                      = 15;  // maximum library storage module number
  static MIN_PANEL                    = 0;   // minimum panel number
  static MAX_PANEL                    = 19;  // maximum panel number

  constructor() {
    super(StkCSI.PROGRAM, StkCSI.VERSION);
    this.driveMap = {};
    this.volumeMap = {};
    this.freeCells = 16;
    this.udpPort = StkCSI.RPC_PORT;
    this.tapeLibraryRoot = "./tapes";
    this.tapeServerClients = [];
    this.tapeServerPort = 4401;
  }

  dump(data) {
    let s = "";
    let i = 0;
    for (let d of data) {
      if (i++ % 16 === 0) s += "\n";
      s += " ";
      if (d < 0x10) s += "0";
      s += d.toString(16);
    }
    console.log(s);
  }

  appendCsiHeader(rpcReply, csiHeader) {
    rpcReply.appendFixedOpaque(csiHeader.xid);
    rpcReply.appendUnsignedInt(csiHeader.ssiId);
    rpcReply.appendUnsignedInt(csiHeader.syntax);
    rpcReply.appendUnsignedInt(csiHeader.proto);
    rpcReply.appendUnsignedInt(csiHeader.ctype);
    rpcReply.appendFixedOpaque(csiHeader.handle);
  }

  appendDriveId(response, driveId) {
    response.appendUnsignedInt(driveId.acs);
    response.appendUnsignedInt(driveId.lsm);
    response.appendUnsignedInt(driveId.panel);
    response.appendUnsignedInt(driveId.drive);
  }

  appendExtraInts(response, count) {
    for (let i = 0; i < count; i++) response.appendUnsignedInt(0);
  }

  appendMsgHeader(response, msgHeader) {
    response.appendUnsignedInt(msgHeader.packetId);
    response.appendEnum(msgHeader.command);
    response.appendUnsignedInt(msgHeader.options);
    if ((msgHeader.options & 0x80) !== 0) {
      response.appendUnsignedInt(msgHeader.extVersion);
      response.appendUnsignedInt(msgHeader.extOptions);
      response.appendUnsignedInt(msgHeader.extLockId);
      response.appendString(msgHeader.username);
      response.appendString(msgHeader.password);
    }
  }

  calculateDriveKey(driveId) {
    return `M${driveId.lsm}P${driveId.panel}D${driveId.drive}`;
  }

  extractCsiHeader(rpcCall) {
    return {
      xid:    rpcCall.params.extractFixedOpaque(32), 
      ssiId:  rpcCall.params.extractUnsignedInt(), 
      syntax: rpcCall.params.extractUnsignedInt(),
      proto:  rpcCall.params.extractUnsignedInt(),
      ctype:  rpcCall.params.extractUnsignedInt(),
      handle: rpcCall.params.extractFixedOpaque(28)
    };
  }

  extractDriveId(rpcCall) {
    return {
      acs:   rpcCall.params.extractUnsignedInt(),
      lsm:   rpcCall.params.extractUnsignedInt(),
      panel: rpcCall.params.extractUnsignedInt(),
      drive: rpcCall.params.extractUnsignedInt()
    };
  }

  extractMsgHeader(rpcCall) {
    let msgHeader = {
      packetId: rpcCall.params.extractUnsignedInt(), 
      command:  rpcCall.params.extractEnum(),
      options:  rpcCall.params.extractUnsignedInt()
    };
    if ((msgHeader.options & 0x80) !== 0) {
      msgHeader.extVersion = rpcCall.params.extractUnsignedInt();
      msgHeader.extOptions = rpcCall.params.extractUnsignedInt();
      msgHeader.extLockId  = rpcCall.params.extractUnsignedInt();
      msgHeader.username   = rpcCall.params.extractString();
      msgHeader.password   = rpcCall.params.extractString();
    }
    return msgHeader;
  }

  deserializeHandle(handle) {
    let obj = {};
    let idx = 0;
    obj.prog  = handle[idx++] << 24;
    obj.prog |= handle[idx++] << 16;
    obj.prog |= handle[idx++] <<  8;
    obj.prog |= handle[idx++];
    obj.vers  = handle[idx++] << 24;
    obj.vers |= handle[idx++] << 16;
    obj.vers |= handle[idx++] <<  8;
    obj.vers |= handle[idx++];
    obj.proc  = handle[idx++] << 24;
    obj.proc |= handle[idx++] << 16;
    obj.proc |= handle[idx++] <<  8;
    obj.proc |= handle[idx++];
    idx += 3;
    obj.proto = handle[idx++];
    idx += 2;
    obj.port |= handle[idx++] <<  8;
    obj.port |= handle[idx++];
    obj.host = `${handle[idx]}.${handle[idx+1]}.${handle[idx+2]}.${handle[idx+3]}`;
    return obj;
  }

  dismountVolume(driveKey, volId, isForce) {
    let status = StkCSI.STATUS_SUCCESS;
    if (typeof this.driveMap[driveKey] === "undefined"
             || typeof this.driveMap[driveKey].volId === "undefined") {
      status = StkCSI.STATUS_DRIVE_AVAILABLE;
    }
    else if (this.driveMap[driveKey].volId !== volId && isForce === false) {
      status = StkCSI.STATUS_VOLUME_NOT_IN_DRIVE;
    }
    else {
      volId = this.driveMap[driveKey].volId;
      if (typeof this.volumeMap[volId] === "object") {
        let volume = this.volumeMap[volId];
        if (typeof volume.fd !== "undefined") {
          fs.closeSync(volume.fd);
          delete volume.fd;
        }
        delete volume.driveKey;
      }
      delete this.driveMap[driveKey].volId;
      let client = this.getClientForDriveKey(driveKey);
      if (client !== null) {
        this.sendResponse(client, `103 ${volId} dismounted from ${driveKey}`);
        console.log(`${new Date().toLocaleString()} ${volId} dismounted from ${driveKey}`);
      }
    }
    return status;
  }

  mountVolume(driveKey, volId) {
    let status = StkCSI.STATUS_SUCCESS;
    if (typeof this.volumeMap[volId] === "undefined") {
      status = StkCSI.STATUS_VOLUME_NOT_IN_LIBRARY;
    }
    else if (typeof this.driveMap[driveKey] !== "undefined"
             && typeof this.driveMap[driveKey].volId !== "undefined") {
      if (this.driveMap[driveKey].volId === volId) {
        status = StkCSI.STATUS_VOLUME_IN_DRIVE;
      }
      else {
        status = StkCSI.STATUS_DRIVE_IN_USE;
      }
    }
    else {
      let volume = this.volumeMap[volId];
      if (typeof volume.driveKey !== "undefined") {
        if (typeof this.driveMap[driveKey] !== "undefined"
            && typeof this.driveMap[driveKey].volId !== "undefined"
            && this.driveMap[driveKey].volId === volId) {
          status = StkCSI.STATUS_VOLUME_IN_DRIVE;
        }
        else {
          status = StkCSI.STATUS_VOLUME_IN_USE;
        }
      }
      else {
        let fd = fs.openSync(`${this.tapeLibraryRoot}/${volume.path}`, volume.writeEnabled ? "r+" : "r", 0o666);
        if (fd === -1) {
          status = StkCSI.STATUS_VOLUME_NOT_IN_LIBRARY;
        }
        else {
          volume.fd = fd;
          volume.position = 0;
          volume.blockId = 0;
          volume.driveKey = driveKey;
          if (typeof this.driveMap[driveKey] === "undefined") {
            this.driveMap[driveKey] = {};
          }
          this.driveMap[driveKey].volId = volId;
          let client = this.getClientForDriveKey(driveKey);
          if (client !== null) {
            this.sendResponse(client, `${volume.writeEnabled ? "102" : "101"} ${volId} mounted on ${driveKey}`);
            console.log(`${new Date().toLocaleString()} ${volId} mounted on ${driveKey}`);
          }
        }
      }
    }
    return status;
  }

  processDismountCommand(rpcCall, options, response) {
    let volId = rpcCall.params.extractString();
    let driveId = this.extractDriveId(rpcCall);
    let driveKey = this.calculateDriveKey(driveId);
    let isForce = (options & StkCSI.FORCE) !== 0;
    let status = StkCSI.STATUS_SUCCESS;
    this.appendDriveId(response, driveId);
    if (driveId.acs !== 0) {
      status = StkCSI.STATUS_ACS_NOT_IN_LIBRARY;
    }
    else {
      status = this.dismountVolume(driveKey, volId, isForce);
    }
    response.appendEnum(status);
    response.appendEnum(StkCSI.TYPE_VOLUME);
    this.appendExtraInts(response, 11);
    response.appendString(volId);
    this.appendDriveId(response, driveId);
    this.debugLog(`StkCSI RPC dismount volume ${volId} from drive ${driveKey}, status ${status}`);
  }

  processMountCommand(rpcCall, response) {
    let volId = rpcCall.params.extractString();
    let count = rpcCall.params.extractUnsignedInt();
    let driveId = this.extractDriveId(rpcCall);
    let driveKey = this.calculateDriveKey(driveId);
    let status = StkCSI.STATUS_SUCCESS;
    if (count < 1) {
      status = StkCSI.STATUS_COUNT_TOO_SMALL;
    }
    else if (count > 1) {
      status = StkCSI.STATUS_COUNT_TOO_LARGE;
    }
    else if (driveId.acs !== 0) {
      status = StkCSI.STATUS_ACS_NOT_IN_LIBRARY;
    }
    else {
      status = this.mountVolume(driveKey, volId);
    }
    response.appendEnum(status);
    response.appendEnum(StkCSI.TYPE_VOLUME);
    this.appendExtraInts(response, 11);
    response.appendString(volId);
    this.appendDriveId(response, driveId);
    this.debugLog(`StkCSI RPC mount volume ${volId} on drive ${driveKey}, status ${status}`);
  }

  processQueryCommand(rpcCall, response) {
    let queryType = rpcCall.params.extractEnum();
    let count = rpcCall.params.extractUnsignedInt();
    //
    // ATF queries only the status of the server and specifies count as 0
    //
    if (queryType === StkCSI.TYPE_SERVER) {
      response.appendEnum(StkCSI.STATUS_SUCCESS);
      response.appendEnum(queryType);
      this.appendExtraInts(response, 8);
      response.appendEnum(StkCSI.TYPE_SERVER);
      response.appendUnsignedInt(1);
      response.appendEnum(StkCSI.STATE_RUN);
      response.appendUnsignedInt(this.freeCells);
      for (let row = 0; row < 2; row++) {   // current and pending requests
        for (let col = 0; col < 5; col++) { // audit, mount, dismount, enter, eject
          response.appendUnsignedInt(0);
        }
      }
      this.debugLog(`StkCSI RPC query type "server", status ${StkCSI.STATUS_SUCCESS}, state "run"`);
    }
    else {
      response.appendEnum(StkCSI.STATUS_UNSUPPORTED_TYPE);
      response.appendEnum(queryType);
      this.appendExtraInts(response, 8);
      response.appendEnum(queryType);
      response.appendUnsignedInt(0);
      this.debugLog(`StkCSI RPC query type ${queryType}, status ${StkCSI.STATUS_UNSUPPORTED_TYPE}`);
    }
  }

  callRpcCallback(host, port, prog, vers, proc, args, retryCount) {
    if (++retryCount <= 5) {
      let rpc = new RPC();
      rpc.setDebug(this.isDebug);
      this.debugLog(`StkCSI callback prog ${prog} vers ${vers} proc ${proc} at ${host}:${port} (try ${retryCount})`);
      rpc.callProcedure(host, ProgramRegistry.IPPROTO_UDP, port, prog, vers, proc, args, result => {
        if (result.isTimeout) {
          this.callRpcCallback(host, port, prog, vers, proc, args, retryCount);
        }
        else if (result.isSuccess) {
          this.debugLog(`StkCSI  success prog ${prog} vers ${vers} proc ${proc} at ${host}:${port}`);
        }
        else {
          this.debugLog(`StkCSI  failure prog ${prog} vers ${vers} proc ${proc} at ${host}:${port}`);
          this.debugLog(`        ${JSON.stringify(result)}`);
        }
      });
    }
    else {
      this.debugLog(`StkCSI  failure prog ${prog} vers ${vers} proc ${proc} at ${host}:${port}`);
      this.debugLog("        maximum retry attempts exceeded");
    }
  }

  processCsiRequest(rpcCall, rpcReply, callback) {
    rpcReply.appendEnum(Program.SUCCESS);
    callback(rpcReply);
    let csiHeader = this.extractCsiHeader(rpcCall);
    let msgHeader = this.extractMsgHeader(rpcCall);
    let response = new XDR();
    msgHeader.options &= (0x80 | StkCSI.FORCE); // preserve extended header and force flags and indicate final response
    this.appendCsiHeader(response, csiHeader);
    this.appendMsgHeader(response, msgHeader);
    switch (msgHeader.command) {
    case StkCSI.COMMAND_QUERY:
      this.processQueryCommand(rpcCall, response);
      break;
    case StkCSI.COMMAND_MOUNT:
      this.processMountCommand(rpcCall, response);
      break;
    case StkCSI.COMMAND_DISMOUNT:
      this.processDismountCommand(rpcCall, msgHeader.options, response);
      break;
    default:
      response.appendEnum(StkCSI.STATUS_INVALID_COMMAND);
      this.debugLog(`StkCSI RPC unsupported command ${msgHeader.command}, status ${StkCSI.STATUS_INVALID_COMMAND}`);
      break;
    }
    setTimeout(() => {
      let obj = this.deserializeHandle(csiHeader.handle);
      this.callRpcCallback(obj.host, obj.port, obj.prog, obj.vers, obj.proc, response.getData(), 0);
    }, 0);
  }

  callProcedure(rpcCall, rpcReply, callback) {
    switch (rpcCall.proc) {
    case 0: // null procedure
      rpcReply.appendEnum(Program.SUCCESS);
      break;
    case 1000: // CSI_ACSLM_PROC
      this.processCsiRequest(rpcCall, rpcReply, callback);
      return;
    default:
      rpcReply.appendEnum(Program.PROC_UNAVAIL);
      break;
    }
    callback(rpcReply);
  }

  addTapeServerClient(socket) {
    this.tapeServerClients.push({
      client: socket,
      data: Buffer.allocUnsafe(0)
    });
  }

  advanceTapePosition(volume, recordLength) {
    let result = this.readRecordLength(volume, volume.position + recordLength);
    if (result.status != StkCSI.SUCCESS) return result.status;
    if (result.length !== recordLength) {
      recordLength += 1;
      result = this.readRecordLength(volume, volume.position + recordLength);
      if (result.status != StkCSI.SUCCESS) return result.status;
      if (result.length !== recordLength) return StkCSI.TAPE_READ_FAILURE;
    }
    volume.position += recordLength + 4;
    return StkCSI.SUCCESS;
  }

  getClientForDriveKey(driveKey) {
    for (let client of this.tapeServerClients) {
      if (typeof client.driveKey !== "undefined" && client.driveKey === driveKey) return client;
    }
    return null;
  }

  getClientForSocket(socket) {
    let idx = this.tapeServerClients.findIndex(client => {
      return client.client.remoteAddress === socket.remoteAddress && client.client.remotePort === socket.remotePort;
    });
    if (idx !== -1) {
      let client = this.tapeServerClients[idx];
      client.index = idx;
      return client;
    }
    return null;
  }

  getVolumeForClient(client) {
    if (typeof client.driveKey !== "undefined" && typeof this.driveMap[client.driveKey] !== "undefined") {
      let drive = this.driveMap[client.driveKey];
      if (typeof drive.volId !== "undefined"
          && typeof this.volumeMap[drive.volId] !== "undefined"
          && typeof this.volumeMap[drive.volId].driveKey !== "undefined"
          && this.volumeMap[drive.volId].driveKey === client.driveKey) {
        return this.volumeMap[drive.volId];
      }
    }
    return null;
  }

  readRecordLength(volume, position) {
    if (typeof volume.fd === "undefined") {
      return {status:StkCSI.END_OF_MEDIUM};
    }
    let buffer = new Uint8Array(4);
    let bytesRead = fs.readSync(volume.fd, buffer, 0, 4, position);
    if (bytesRead < 0) {
      return {status:StkCSI.TAPE_READ_FAILURE};
    }
    else if (bytesRead !== 4) {
      return {status:StkCSI.END_OF_MEDIUM};
    }
    let length = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
    return {status:StkCSI.SUCCESS, length:length};
  }

  removeTapeServerClient(socket) {
    let client = this.getClientForSocket(socket);
    if (client !== null) {
      let volume = this.getVolumeForClient(client);
      if (volume !== null) {
        delete volume.driveKey;
        if (typeof volume.fd !== "undefined") {
          fs.closeSync(volume.fd);
          delete volume.fd;
        }
      }
      if (typeof client.driveKey !== "undefined") delete this.driveMap[client.driveKey];
      this.tapeServerClients.splice(client.index, 1);
    }
  }

  retreatTapePosition(volume, recordLength) {
    let position = volume.position - (recordLength + 4);
    if (position < 0) return StkCSI.TAPE_READ_FAILURE;
    let result = this.readRecordLength(volume, position);
    if (result.status != StkCSI.SUCCESS) return result.status;
    if (result.length !== recordLength) {
      position -= 1;
      result = this.readRecordLength(volume, position);
      if (result.status != StkCSI.SUCCESS) return result.status;
      if (result.length !== recordLength) return StkCSI.TAPE_READ_FAILURE;
    }
    volume.position = position;
    return StkCSI.SUCCESS;
  }

  processDismountRequest(client, request) {
    client.data = Buffer.allocUnsafe(0);
    if (request.length > 1) {
      const volId = request[1];
      let status = this.dismountVolume(client.driveKey, volId, true);
      if (status === StkCSI.STATUS_SUCCESS) {
        this.sendResponse(client, `200 ${volId} dismounted from ${client.driveKey}`);
      }
      else {
        this.sendResponse(client, `402 ${status} ${volId} not dismounted`);
      }
    }
    else {
      this.sendResponse(client, "400 Bad request");
    }
  }

  processLocateBlockRequest(client, request) {
    client.data = Buffer.allocUnsafe(0);
    if (request.length < 2) {
      this.sendResponse(client, "400 Bad request");
      return;
    }
    let blockId = parseInt(request[1]);
    if (isNaN(blockId)) {
      this.sendResponse(client, "400 Bad request");
      return;
    }
    blockId &= 0xfffff;
    let volume = this.getVolumeForClient(client);
    if (volume === null) {
      this.sendResponse(client, "403 No tape mounted");
      return;
    }
    while (blockId > volume.blockId) {
      let result = {};
      while (true) {
        result = this.readRecordLength(volume, volume.position);
        if (result.status != StkCSI.SUCCESS || result.length !== StkCSI.ERASE_GAP) break;
        volume.position += 4;
      }
      if (result.status === StkCSI.TAPE_READ_FAILURE) {
        this.sendResponse(client, "404 Failed to read record length");
        return;
      }
      else if (result.status === StkCSI.END_OF_MEDIUM) {
        this.sendResponse(client, "504 Block not found");
        return;
      }
      else if (result.length === StkCSI.TAPE_MARK) {
        volume.position += 4;
        volume.blockId += 1;
      }
      else if ((result.length & StkCSI.RECORD_ERROR_FLAG) !== 0) {
        this.sendResponse(client, "501 Record error");
        return;
      }
      else {
        volume.position += 4;
        let status = this.advanceTapePosition(volume, result.length);
        if (status != StkCSI.SUCCESS) {
          this.sendResponse(client, "404 Failed to advance tape position");
          return;
        }
        volume.blockId += 1;
      }
    }
    while (blockId < volume.blockId) {
      let result = {};
      let position = volume.position - 4;
      while (position >= 0) {
        result = this.readRecordLength(volume, position);
        if (result.status != StkCSI.SUCCESS || result.length !== StkCSI.ERASE_GAP) break;
        position -= 4;
      }
      if (position < 0) {
        volume.position = 0;
        volume.blockId = 0;
        this.sendResponse(client, "504 Block not found");
        return;
      }
      else if (result.status === StkCSI.TAPE_READ_FAILURE) {
        this.sendResponse(client, "404 Failed to read record length");
        return;
      }
      else if (result.length === StkCSI.TAPE_MARK) {
        volume.position = position;
        volume.blockId -= 1;
      }
      else if ((result.length & StkCSI.RECORD_ERROR_FLAG) !== 0) {
        volume.position = position + 4;
        this.sendResponse(client, "501 Record error");
        return;
      }
      else {
        volume.position = position;
        let status = this.retreatTapePosition(volume, result.length);
        if (status != StkCSI.SUCCESS) {
          volume.position += 4;
          this.sendResponse(client, "404 Failed to retreat tape position");
          return;
        }
        volume.blockId -= 1;
      }
    }
    this.sendResponse(client, "200 Ok");
  }

  processMountRequest(client, request) {
    client.data = Buffer.allocUnsafe(0);
    if (request.length > 1) {
      const volId = request[1];
      let status = this.mountVolume(client.driveKey, volId);
      if (status === StkCSI.STATUS_SUCCESS) {
        this.sendResponse(client, `200 ${volId} mounted on ${client.driveKey}`);
      }
      else {
        this.sendResponse(client, `402 ${status} ${volId} not mounted`);
      }
    }
    else {
      this.sendResponse(client, "400 Bad request");
    }
  }

  processReadBkwRequest(client, request, doReturnData) {
    client.data = Buffer.allocUnsafe(0);
    let volume = this.getVolumeForClient(client);
    if (volume === null) {
      this.sendResponse(client, "403 No tape mounted");
      return;
    }
    let result = {};
    let position = volume.position - 4;
    while (position >= 0) {
      result = this.readRecordLength(volume, position);
      if (result.status != StkCSI.SUCCESS || result.length !== StkCSI.ERASE_GAP) break;
      position -= 4;
    }
    if (position < 0) {
      volume.position = 0;
      volume.blockId = 0;
      this.sendResponse(client, "203 Start of medium");
      return;
    }
    if (result.status === StkCSI.TAPE_READ_FAILURE) {
      this.sendResponse(client, "404 Failed to read record length");
      return;
    }
    if ((result.length & StkCSI.RECORD_ERROR_FLAG) !== 0) {
      volume.position = position + 4;
      this.sendResponse(client, "501 Record error");
      return;
    }
    volume.position = position;
    if (result.length === StkCSI.TAPE_MARK) {
      volume.blockId -= 1;
      this.sendResponse(client, "202 Tape mark");
      return;
    }
    if (position < 4) {
      volume.position = 0;
      volume.blockId = 0;
      this.sendResponse(client, "203 Start of medium");
      return;
    }
    let status = this.retreatTapePosition(volume, result.length);
    if (status != StkCSI.SUCCESS) {
      volume.position = position + 4;
      this.sendResponse(client, "404 Failed to retreat tape position");
      return;
    }
    let buffer = new Uint8Array(result.length);
    let bytesRead = fs.readSync(volume.fd, buffer, 0, result.length, volume.position + 4);
    if (bytesRead < 0) {
      this.sendResponse(client, "404 Failed to read data");
      return;
    }
    if (bytesRead !== result.length) {
      this.sendResponse(client, "501 Failed to read record");
      return;
    }
    volume.blockId -= 1;
    if (doReturnData) {
      this.sendResponse(client, `201 ${result.length} bytes`);
      client.client.write(buffer);
    }
    else {
      this.sendResponse(client, "200 Ok");
    }
  }

  processReadBlockIdRequest(client, request) {
    client.data = Buffer.allocUnsafe(0);
    let volume = this.getVolumeForClient(client);
    if (volume === null) {
      this.sendResponse(client, "403 No tape mounted");
      return;
    }
    let stat = fs.fstatSync(volume.fd);
    let physRefValue = Math.floor((volume.position / stat.size) * 126) + 1;
    let id = (physRefValue << 24) | volume.blockId;
    this.sendResponse(client, `204 ${id} ${id}`);
  }

  processReadFwdRequest(client, request, doReturnData) {
    client.data = Buffer.allocUnsafe(0);
    let volume = this.getVolumeForClient(client);
    if (volume === null) {
      this.sendResponse(client, "403 No tape mounted");
      return;
    }
    let result = {};
    while (true) {
      result = this.readRecordLength(volume, volume.position);
      if (result.status != StkCSI.SUCCESS || result.length !== StkCSI.ERASE_GAP) break;
      volume.position += 4;
    }
    if (result.status === StkCSI.TAPE_READ_FAILURE) {
      this.sendResponse(client, "404 Failed to read record length");
      return;
    }
    if (result.status === StkCSI.END_OF_MEDIUM) {
      this.sendResponse(client, "505 End of medium");
      return;
    }
    if (result.length === StkCSI.TAPE_MARK) {
      volume.position += 4;
      volume.blockId += 1;
      this.sendResponse(client, "202 Tape mark");
      return;
    }
    if ((result.length & StkCSI.RECORD_ERROR_FLAG) !== 0) {
      this.sendResponse(client, "501 Record error");
      return;
    }
    let buffer = new Uint8Array(result.length);
    let bytesRead = fs.readSync(volume.fd, buffer, 0, result.length, volume.position + 4);
    if (bytesRead < 0) {
      this.sendResponse(client, "404 Failed to read data");
      return;
    }
    if (bytesRead !== result.length) {
      this.sendResponse(client, "505 End of medium");
      return;
    }
    let position = volume.position;
    volume.position += 4;
    let status = this.advanceTapePosition(volume, result.length);
    if (status != StkCSI.SUCCESS) {
      volume.position = position;
      this.sendResponse(client, "404 Failed to advance tape position");
      return;
    }
    volume.blockId += 1;
    if (doReturnData) {
      this.sendResponse(client, `201 ${result.length} bytes`);
      client.client.write(buffer);
    }
    else {
      this.sendResponse(client, "200 Ok");
    }
  }

  processRewindRequest(client, request) {
    client.data = Buffer.allocUnsafe(0);
    let volume = this.getVolumeForClient(client);
    if (volume === null) {
      this.sendResponse(client, "403 No tape mounted");
      return;
    }
    volume.position = 0;
    volume.blockId = 0;
    this.sendResponse(client, "203 Start of medium");
  }

  processRegisterRequest(client, request) {
    client.data = Buffer.allocUnsafe(0);
    if (request.length < 2) {
      this.sendResponse(client, "400 Bad request");
      return;
    }
    const driveKey = request[1];
    if (typeof this.driveMap[driveKey] !== "undefined") {
      this.sendResponse(client, `401 ${driveKey} already registered`);
      return;
    }
    this.driveMap[driveKey] = {client: client};
    client.driveKey = driveKey;
    this.sendResponse(client, `200 ${driveKey} registered`);
    console.log(`${new Date().toLocaleString()} ${driveKey} registered by ${client.client.remoteAddress}`);
  }

  processWriteRequest(client, request, dataIndex) {
    let volume = this.getVolumeForClient(client);
    if (volume === null) {
      this.sendResponse(client, "403 No tape mounted");
      client.data = Buffer.allocUnsafe(0);
      return;
    }
    if (request.length < 2) {
      this.sendResponse(client, "400 Bad request");
      client.data = Buffer.allocUnsafe(0);
      return;
    }
    let dataLength = parseInt(request[1]);
    if (isNaN(dataLength)) {
      this.sendResponse(client, "400 Bad request");
      client.data = Buffer.allocUnsafe(0);
      return;
    }
    if (client.data.length - dataIndex < dataLength) return;
    if (volume.writeEnabled !== true) {
      this.sendResponse(client, "502 Volume is read-only");
      client.data = Buffer.allocUnsafe(0);
      return;
    }
    let recordLength = new Uint8Array(4);
    recordLength[0] = dataLength & 0xff;
    recordLength[1] = (dataLength >>  8) & 0xff;
    recordLength[2] = (dataLength >> 16) & 0xff;
    recordLength[3] = (dataLength >> 24) & 0xff;
    if (fs.writeSync(volume.fd, recordLength, 0, 4, volume.position) !== 4) {
      this.sendResponse(client, "503 Failed to write record length");
      client.data = Buffer.allocUnsafe(0);
      return;
    }
    volume.position += 4;
    let bytesWritten = fs.writeSync(volume.fd, client.data, dataIndex, dataLength, volume.position);
    client.data = Buffer.allocUnsafe(0);
    if (bytesWritten !== dataLength) {
      this.sendResponse(client, "503 Failed to write record data");
      return;
    }
    volume.position += dataLength;
    if (fs.writeSync(volume.fd, recordLength, 0, 4, volume.position) !== 4) {
      this.sendResponse(client, "503 Failed to write record length");
      return;
    }
    volume.position += 4;
    this.sendResponse(client, "200 Ok");
  }

  processWriteTapeMarkRequest(client, request) {
    client.data = Buffer.allocUnsafe(0);
    let volume = this.getVolumeForClient(client);
    if (volume === null) {
      this.sendResponse(client, "403 No tape mounted");
      return;
    }
    let tapeMark = new Uint8Array(4);
    tapeMark[0] =  StkCSI.TAPE_MARK & 0xff;
    tapeMark[1] = (StkCSI.TAPE_MARK >>  8) & 0xff;
    tapeMark[2] = (StkCSI.TAPE_MARK >> 16) & 0xff;
    tapeMark[3] = (StkCSI.TAPE_MARK >> 24) & 0xff;
    if (fs.writeSync(volume.fd, tapeMark, 0, 4, volume.position) !== 4) {
      this.sendResponse(client, "503 Failed to write tape mark");
      return;
    }
    volume.position += 4;
    this.sendResponse(client, "200 Ok");
  }

  sendResponse(client, response) {
    client.client.write(`${response}\n`);
    this.debugLog(`StkCSI TCP ${client.client.remoteAddress}:${client.client.remotePort} => ${response}`);
  }

  handleTapeServerRequest(socket, data) {
    let idx = this.tapeServerClients.findIndex(client => {
      return client.client.remoteAddress === socket.remoteAddress && client.client.remotePort === socket.remotePort;
    });
    if (idx !== -1) {
      let client = this.tapeServerClients[idx];
      client.data = Buffer.concat([client.data, data]);
      idx = client.data.indexOf("\n");
      if (idx !== -1) {
        let request = client.data.slice(0, idx).toString().trim();
        this.debugLog(`StkCSI TCP ${client.client.remoteAddress}:${client.client.remotePort} <= ${request}`);
        request = request.split(" ");
        if (request.length > 0) {
          switch (request[0]) {
          case "DISMOUNT":
            this.processDismountRequest(client, request);
            break;
          case "LOCATEBLOCK":
            this.processLocateBlockRequest(client, request);
            break;
          case "MOUNT":
            this.processMountRequest(client, request);
            break;
          case "READBKW":
            this.processReadBkwRequest(client, request, true);
            break;
          case "READBLOCKID":
            this.processReadBlockIdRequest(client, request);
            break;
          case "READFWD":
            this.processReadFwdRequest(client, request, true);
            break;
          case "REGISTER":
            this.processRegisterRequest(client, request);
            break;
          case "REWIND":
            this.processRewindRequest(client, request);
            break;
          case "SPACEBKW":
            this.processReadBkwRequest(client, request, false);
            break;
          case "SPACEFWD":
            this.processReadFwdRequest(client, request, false);
            break;
          case "WRITE":
            this.processWriteRequest(client, request, idx + 1);
            break;
          case "WRITEMARK":
            this.processWriteTapeMarkRequest(client, request);
            break;
          default:
            this.sendResponse(client, `401 ${request[0]}?`);
            client.data = Buffer.allocUnsafe(0);
            break;
          }
        }
      }
    }
  }

  setTapeLibraryRoot(tapeLibraryRoot) {
    this.tapeLibraryRoot = fs.realpathSync(tapeLibraryRoot);
  }

  setTapeServerPort(tapeServerPort) {
    this.tapeServerPort = tapeServerPort;
  }

  setVolumeMap(volumeMap) {
    this.volumeMap = volumeMap;
  }

  start() {
    super.start();
    const me = this;
    const tapeServer = net.createServer(client => {
      this.debugLog(`StkCSI TCP ${client.remoteAddress}:${client.remotePort} connected`);
      me.addTapeServerClient(client);
      client.on("end", () => {
        this.debugLog(`StkCSI TCP ${client.remoteAddress}:${client.remotePort} disconnected`);
        me.removeTapeServerClient(client);
      });
      client.on("data", data => {
        me.handleTapeServerRequest(client, data);
      });
    });
    tapeServer.listen(this.tapeServerPort, () => {
      console.log(`${new Date().toLocaleString()} Tape library located at ${this.tapeLibraryRoot}`);
      console.log(`${new Date().toLocaleString()} Tape server listening on port ${this.tapeServerPort}`);
    });
  }
}

module.exports = StkCSI;
