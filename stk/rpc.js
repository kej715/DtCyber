const crypto = require("crypto");
const dgram = require("dgram");
const Program = require("./program");
const ProgramRegistry = require("./registry");
const XDR = require("./xdr");

class RPC {

  constructor() {
    this.isDebug = false;
    this.responseTimeout = 5000;
  }

  setDebug(isDebug) {
    this.isDebug = isDebug;
  }

  setResponseTime(millis) {
    this.responseTimeout = millis;
  }

  debugLog(msg) {
    if (this.isDebug) {
      console.log(`${new Date().toLocaleString()} ${msg}`);
    }
  }

  callProcedure(host, prot, port, prog, vers, proc, args, callback) {
    let rpcCall = new XDR();
    const xid = crypto.randomInt(0x100000000);
    rpcCall.appendUnsignedInt(xid);
    rpcCall.appendEnum(Program.CALL);
    rpcCall.appendUnsignedInt(2);
    rpcCall.appendUnsignedInt(prog);
    rpcCall.appendUnsignedInt(vers);
    rpcCall.appendUnsignedInt(proc);
    rpcCall.appendEnum(Program.AUTH_NONE); // cred flavor
    rpcCall.appendVariableOpaque([]);      // cred body
    rpcCall.appendEnum(Program.AUTH_NONE); // verf flavor
    rpcCall.appendVariableOpaque([]);      // verf body
    rpcCall.appendFixedOpaque(args);
    if (prot === ProgramRegistry.IPPROTO_UDP) {
      const client = dgram.createSocket('udp4');
      let timer = setTimeout(() => {
        client.close();
        callback({isSuccess: false, reason: "timeout"});
      }, this.responseTimeout);
      client.on("error", err => {
        clearTimeout(timer);
        client.close();
        callback({isSuccess: false, reason: err});
      });
      client.on("message", (msg, rinfo) => {
        let rpcReply = new XDR();
        rpcReply.setData(msg);
        let rcvXid = rpcReply.extractUnsignedInt();
        let mtype = rpcReply.extractEnum();
        if (rcvXid === xid && mtype === Program.REPLY) {
          clearTimeout(timer);
          client.close();
          let result = {};
          let replyStat = rpcReply.extractEnum();
          if (replyStat === Program.MSG_ACCEPTED) {
            result.isAccepted = true;
            result.verf = {};
            result.verf.flavor = rpcReply.extractEnum();
            result.verf.body = rpcReply.extractVariableOpaque();
            result.acceptStatus = rpcReply.extractEnum();
            switch (result.acceptStatus) {
            case Program.SUCCESS:
              result.isSuccess = true;
              result.result = rpcReply.extractRemainder();
              break;
            case Program.PROG_MISMATCH:
              result.lowVersion = rpcReply.extractUnsignedInt();
              result.highVersion = rpcReply.extractUnsignedInt();
              // fall through
            default:
              result.isSuccess = false;
              break;
            }
          }
          else {
            result = {isSuccess: false, isAccepted: false};
            result.rejectStatus = rpcReply.extractEnum();
            switch (result.rejectStatus) {
            case Program.RPC_MISMATCH:
              result.lowVersion = rpcReply.extractUnsignedInt();
              result.highVersion = rpcReply.extractUnsignedInt();
              break;
            case Program.AUTH_ERROR:
              result.authStatus = rpcReply.extractEnum();
              break;
            }
          }
          callback(result);
        }
        else {
          this.debugLog(`RPC.callProcedure: received unexpected mtype or xid: mtype ${mtype}, expected xid ${xid}, received xid ${rcvXid}`);
        }
      });
      client.send(rpcCall.getData(), port, host);
    }
    else {
      this.debugLog(`RPC.callProcedure: unsupported protocol: ${prot}`);
      callback({isSuccess: false, reason: `unsupported protocol ${prot}`});
    }
  }
}

module.exports = RPC;
