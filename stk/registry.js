/*
 * This module provides a class that implements an RPC program registry.
 */
class ProgramRegistry {

  static IPPROTO_TCP = 6;  /* protocol number for TCP/IP */
  static IPPROTO_UDP = 17; /* protocol number for UDP/IP */

  constructor() {
    this.registry = {};
  }

  calculateRegistryKey(prog, vers) {
    return `p${prog}v${vers}`;
  }

  registerProgram(program) {
    this.registry[this.calculateRegistryKey(program.prog, program.vers)] = program;
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
          prot: ProgramRegistry.IPPROTO_UDP,
          port: entry.udpPort
        });
      }
      if (typeof entry.tcpPort !== "undefined") {
        list.push({
          prog: entry.prog,
          vers: entry.vers,
          prot: ProgramRegistry.IPPROTO_TCP,
          port: entry.tcpPort
        });
      }
    }
    return list;
  }
}

module.exports = ProgramRegistry;
