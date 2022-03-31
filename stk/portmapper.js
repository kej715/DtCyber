const Const = require("./const");
const Global = require("./global");
const Program = require("./program");
const RPC = require("./rpc");

class PortMapper extends Program {

  static PROC_SET = 1;      /* Portmapper SET procedure number */
  static PROGRAM  = 100000; /* Portmapper program number */
  static VERSION  = 2;      /* Portmapper version number */

  constructor() {
    super(PortMapper.PROGRAM, PortMapper.VERSION);
    this.udpPort = 111;
  }

  dumpProgramList(rpcReply) {
    let list = Global.programRegistry.listPrograms();
    rpcReply.appendEnum(Program.SUCCESS);
    for (let mapping of list) {
      rpcReply.appendBoolean(true);
      rpcReply.appendUnsignedInt(mapping.prog);
      rpcReply.appendUnsignedInt(mapping.vers);
      rpcReply.appendUnsignedInt(mapping.prot);
      rpcReply.appendUnsignedInt(mapping.port);
    }
    rpcReply.appendBoolean(false);
  }

  getPort(rpcCall, rpcReply) {
    let prog = rpcCall.params.extractUnsignedInt();
    let vers = rpcCall.params.extractUnsignedInt();
    let prot = rpcCall.params.extractUnsignedInt();
    let port = rpcCall.params.extractUnsignedInt();
    let program = Global.programRegistry.lookupProgram(prog, vers);
    port = 0;
    if (typeof program === "object") {
      if (prot === Const.IPPROTO_UDP && typeof program.udpPort !== "undefined") {
        port = program.udpPort;
      }
      else if (prot === Const.IPPROTO_TCP && typeof program.tcpPort !== "undefined") {
        port = program.tcpPort;
      }
    }
    rpcReply.appendEnum(Program.SUCCESS);
    rpcReply.appendUnsignedInt(port);
  }

  registerProgram(rpcCall, rpcReply) {
    let prog = rpcCall.params.extractUnsignedInt();
    let vers = rpcCall.params.extractUnsignedInt();
    let prot = rpcCall.params.extractUnsignedInt();
    let port = rpcCall.params.extractUnsignedInt();
    let result = true;
    rpcReply.appendEnum(Program.SUCCESS);
    let program = Global.programRegistry.lookupProgram(prog, vers);
    if (typeof program === "object") {
      if ((prot === Const.IPPROTO_UDP && typeof program.udpPort !== "undefined")
          || (prot === Const.IPPROTO_TCP && typeof program.tcpPort !== "undefined")) {
        result = false;
      }
    }
    else {
      program = new Program(prog, vers);
      Global.programRegistry.registerProgram(program);
    }
    if (prot === Const.IPPROTO_UDP) {
      program.setUdpPort(port);
    }
    else if (prot === Const.IPPROTO_TCP) {
      program.setTcpPort(port);
    }
    rpcReply.appendEnum(Program.SUCCESS);
    rpcReply.appendBoolean(result);
  }

  unregisterProgram(rpcCall, rpcReply) {
    let prog = rpcCall.params.extractUnsignedInt();
    let vers = rpcCall.params.extractUnsignedInt();
    let prot = rpcCall.params.extractUnsignedInt();
    let port = rpcCall.params.extractUnsignedInt();
    Global.programRegistry.unregisterProgram(prog, vers);
    rpcReply.appendEnum(Program.SUCCESS);
    rpcReply.appendBoolean(true);
  }

  callIt(rpcCall, rpcReply, callback) {
    let prog = rpcCall.params.extractUnsignedInt();
    let vers = rpcCall.params.extractUnsignedInt();
    let proc = rpcCall.params.extractUnsignedInt();
    let args = rpcCall.params.extractRemainder();
    let program = Global.programRegistry.lookupProgram(prog, vers);
    if (typeof program === "object" && typeof program.udpPort !== "undefined") {
      let rpc = new RPC();
      rpc.callProcedure("localhost", Const.IPPROTO_UDP, program.udpPort, prog, vers, proc, args, reply => {
        if (reply.isSuccess) {
          rpcReply.appendEnum(Program.SUCCESS);
          rpcReply.appendUnsignedInt(program.udpPort);
          rpcReply.appendVariableOpaque(result.result);
          callback(rpcReply);
        }
      });
    }
  }

  callProcedure(rpcCall, rpcReply, callback) {
    switch (rpcCall.proc) {
    case 0: // null procedure
      rpcReply.appendEnum(Program.SUCCESS);
      break;
    case 1: // set mapping
      this.registerProgram(rpcCall, rpcReply);
      break;
    case 2: // unset mapping
      this.unregisterProgram(rpcCall, rpcReply);
      break;
    case 3: // get port
      this.getPort(rpcCall, rpcReply);
      break;
    case 4: // dump
      this.dumpProgramList(rpcReply);
      break;
    case 5: // call it
      this.callIt(rpcCall, rpcReply, callback);
      return;
    default:
      rpcReply.appendEnum(Program.PROC_UNAVAIL);
      break;
    }
    callback(rpcReply);
  }
 
  setUdpPort(udpPort) {
    this.udpPort = udpPort;
  }
}

module.exports = PortMapper;
