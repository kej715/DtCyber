/*
 * This module provides a class that implements an RPC program registry.
 */
const Const = require("./const");
const PortMapper = require("./portmapper");
const RPC = require("./rpc");
const XDR = require("./xdr");

class ProgramRegistry {

  constructor() {
    this.registry = {};
    this.isDebug = false;
  }

  calculateRegistryKey(prog, vers) {
    return `p${prog}v${vers}`;
  }

  callForeignPortMapper(program, retryCount, callback) {
    if (++retryCount <= 5) {
      let args = new XDR();
      args.appendUnsignedInt(program.prog);
      args.appendUnsignedInt(program.vers);
      args.appendUnsignedInt(Const.IPPROTO_UDP);
      args.appendUnsignedInt(program.udpPort);
      let rpc = new RPC();
      rpc.setDebug(this.isDebug);
      if (this.isDebug) {
        console.log(`${new Date().toLocaleString()} ProgramRegistry call portmapper at ${this.foreignPortMapper.host}:${this.foreignPortMapper.port} to set prog ${program.prog} vers ${program.vers} port ${program.udpPort} (try ${retryCount})`);
      }
      rpc.callProcedure(this.foreignPortMapper.host, Const.IPPROTO_UDP, this.foreignPortMapper.port,
        PortMapper.PROGRAM, PortMapper.VERSION, PortMapper.PROC_SET, args.getData(), result => {
        if (result.isTimeout) {
          this.callForeignPortMapper(program, retryCount, callback);
        }
        else {
          callback(result);
        }
      });
    }
    else {
      callback({isSuccess:false, reason:"Maximum timeout retries exceeded"});
    }
  }

  registerProgram(program) {
    this.registry[this.calculateRegistryKey(program.prog, program.vers)] = program;
    if (typeof this.foreignPortMapper !== "undefined") {
      this.callForeignPortMapper(program, 0, result => {
        if (result.isSuccess === false) {
          console.log(`${new Date().toLocaleString()} ProgramRegistry failed to register prog ${program.prog} vers ${program.vers} at ${this.foreignPortMapper.host}:${this.foreignPortMapper.port}`);
          console.log(`${new Date().toLocaleString()} ProgramRegistry result: ${JSON.stringify(result)}`);
        }
        else if (this.isDebug) {
          console.log(`${new Date().toLocaleString()} ProgramRegistry prog ${program.prog} vers ${program.vers} registered at ${this.foreignPortMapper.host}:${this.foreignPortMapper.port}`);
        }
      });
    }
  }

  unregisterProgram(prog, vers) {
    delete this.registry[this.calculateRegistryKey(prog, vers)];
  }

  lookupProgram(prog, vers) {
    return this.registry[this.calculateRegistryKey(prog, vers)];
  }

  listPrograms() {
    let list = [];
    for (let entry of Object.values(this.registry)) {
      if (typeof entry.udpPort !== "undefined") {
        list.push({
          prog: entry.prog,
          vers: entry.vers,
          prot: Const.IPPROTO_UDP,
          port: entry.udpPort
        });
      }
      if (typeof entry.tcpPort !== "undefined") {
        list.push({
          prog: entry.prog,
          vers: entry.vers,
          prot: Const.IPPROTO_TCP,
          port: entry.tcpPort
        });
      }
    }
    return list;
  }

  setDebug(isDebug) {
    this.isDebug = isDebug;
  }

  getForeignPortMapper() {
    return this.foreignPortMapper;
  }

  setForeignPortMapper(portMapper) {
    if (typeof portMapper === "string") {
      let parts = portMapper.split(":");
      if (parts.length < 2) {
        this.foreignPortMapper = {host:portMapper, port:111};
      }
      else {
        this.foreignPortMapper = {host:parts[0], port:parseInt(parts[1])};
      }
    }
    else {
      this.foreignPortMapper = portMapper;
    }
  }
}

module.exports = ProgramRegistry;
