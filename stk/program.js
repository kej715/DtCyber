/*
 * This module provides the base class used in implementing RPC programs.
 * RPC programs are implemented as extensions of this class. It implements
 * ONCRPC behavior in conformance with IETF specification:
 *
 *     https://tools.ietf.org/html/draft-ietf-oncrpc-remote-03
 */
const dgram = require('dgram');
const ProgramRegistry = require('./registry');
const XDR = require("./xdr");

class Program {
  /*
   * RPC message types
   */
  static CALL          = 0;
  static REPLY         = 1;
  /*
   * RPC reply types
   */
  static MSG_ACCEPTED  = 0;
  static MSG_DENIED    = 1;
  /*
   * accepted message status
   */
  static SUCCESS       = 0; /* RPC executed successfully             */
  static PROG_UNAVAIL  = 1; /* remote hasn't exported program        */
  static PROG_MISMATCH = 2; /* remote can't support version #        */
  static PROC_UNAVAIL  = 3; /* program can't support procedure       */
  static GARBAGE_ARGS  = 4; /* procedure can't decode params         */
  static SYSTEM_ERR    = 5; /* errors like memory allocation failure */
  /*
   * denied message reasons
   */
  static RPC_MISMATCH  = 0;  /* RPC version number != 2              */
  static AUTH_ERROR    = 1;  /* remote can't authenticate caller     */
  /*
   * accepted authentication status
   */
  static AUTH_OK           = 0;  /* success                          */
  /*
   * failed authentication at remote end
   */
  static AUTH_BADCRED      = 1;  /* bad credential (seal broken)     */
  static AUTH_REJECTEDCRED = 2;  /* client must begin new session    */
  static AUTH_BADVERF      = 3;  /* bad verifier (seal broken)       */
  static AUTH_REJECTEDVERF = 4;  /* verifier expired or replayed     */
  static AUTH_TOOWEAK      = 5;  /* rejected for security reasons    */
  /*
   * failed locally
   */
  static AUTH_INVALIDRESP  = 6;  /* bogus response verifier          */
  static AUTH_FAILED       = 7;  /* reason unknown                   */
  /*
   * kerberos errors
   */
  static AUTH_KERB_GENERIC = 8;  /* kerberos generic error           */
  static AUTH_TIMEEXPIRE   = 9;  /* time of credential expired       */
  static AUTH_TKT_FILE     = 10; /* something wrong with ticket file */
  static AUTH_DECODE       = 11; /* can't decode authenticator       */
  static AUTH_NET_ADDR     = 12; /* wrong net address in ticket      */
  /*
   * authentication types
   */
  static AUTH_NONE         = 0;
  static AUTH_SYS          = 1;
  static AUTH_SHORT        = 2;
  static AUTH_DH           = 3;  /* Diffie-Hellman Authentication    */
  static AUTH_KERB4        = 4;  /* Kerberos V4 Authentication       */
  static AUTH_RSA          = 5;  /* (experimental) RSA-based Authentication */
  static RPCSEC_GSS        = 6;  /* GSS-API based Security           */

  static programRegistry = new ProgramRegistry();
  static tcpPortsInUse = [];
  static udpPortsInUse = [];

  constructor(prog, vers) {
    this.prog = prog;
    this.vers = vers;
  }

  setTcpPort(port) {
    this.tcpPort = port;
  }

  setUdpPort(port) {
    this.udpPort = port;
  }

  extractCall(msg) {
    const xdr = new XDR();
    xdr.setData(msg);
    let rpcCall = {};
    rpcCall.xid = xdr.extractUnsignedInt();
    rpcCall.mtype = xdr.extractEnum();
    rpcCall.rpcvers = xdr.extractUnsignedInt();
    rpcCall.prog = xdr.extractUnsignedInt();
    rpcCall.vers = xdr.extractUnsignedInt();
    rpcCall.proc = xdr.extractUnsignedInt();
    rpcCall.auth = {};
    rpcCall.auth.flavor = xdr.extractEnum();
    rpcCall.auth.body = xdr.extractVariableOpaque();
    rpcCall.verf = {};
    rpcCall.verf.flavor = xdr.extractEnum();
    rpcCall.verf.body = xdr.extractVariableOpaque();
    rpcCall.params = xdr;
    return rpcCall;
  }

  callProcedure(rpcCall, rpcReply, callback) {
    //
    // This method must be overridden by subclasses. The default implementation
    //  returns an error indication.
    //
    rpcReply.appendEnum(Program.PROC_UNAVAIL);
    callback(rpcReply);
  }

  handleCallRequest(rpcCall, callback) {
    let rpcReply = new XDR();
    rpcReply.appendUnsignedInt(rpcCall.xid);
    rpcReply.appendEnum(Program.REPLY);
    if (rpcCall.rpcvers !== 2) {
      rpcReply.appendEnum(Program.MSG_DENIED);
      rpcReply.appendEnum(Program.RPC_MISMATCH);
      rpcReply.appendUnsignedInt(2);
      rpcReply.appendUnsignedInt(2);
      callback(rpcReply);
      return;
    }
    let program = Program.programRegistry.lookupProgram(rpcCall.prog, rpcCall.vers);
    if (typeof program !== "object"
        || typeof program.udpPort === "undefined"
        || program.udpPort !== this.udpPort) {
      rpcReply.appendEnum(Program.MSG_ACCEPTED);
      rpcReply.appendEnum(Program.AUTH_NONE);
      rpcReply.appendVariableOpaque([]);
      rpcReply.appendEnum(Program.PROG_UNAVAIL);
      callback(rpcReply);
    }
    // TODO: validate auth info
    rpcReply.appendEnum(Program.MSG_ACCEPTED);
    rpcReply.appendEnum(Program.AUTH_NONE);
    rpcReply.appendVariableOpaque([]);
    program.callProcedure(rpcCall, rpcReply, callback);
  }

  startUdp() {
    this.socket = dgram.createSocket('udp4');
    this.socket.on('error', err => {
      console.log(`${new Date().toLocaleString()} ${err.stack}`);
      this.socket.close();
      let idx = Program.udpPortsInUse.indexOf(this.udpPort);
      if (idx !== -1) {
        Program.udpPortsInUse.splice(idx, 1);
      }
    });
    this.socket.on('message', (msg, rinfo) => {
      console.log(`${new Date().toLocaleString()} received UDP packet from ${rinfo.address}:${rinfo.port}`);
      let rpcCall = this.extractCall(msg);
      console.log(`${new Date().toLocaleString()}   xid ${rpcCall.xid}, mtype ${rpcCall.mtype}, rpcvers ${rpcCall.rpcvers}, prog ${rpcCall.prog}, vers ${rpcCall.vers}, proc: ${rpcCall.proc}`);
      if (rpcCall.mtype !== Program.CALL) {
        console.log(`${new Date().toLocaleString()} message type is not RPC CALL`);
        return;
      }
      this.handleCallRequest(rpcCall, rpcReply => {
        this.socket.send(rpcReply.getData(), rinfo.port, rinfo.address, () => {
          console.log(`${new Date().toLocaleString()} replied to ${rinfo.address}:${rinfo.port}`);
        });
      });
    });
    this.socket.on('listening', () => {
      const address = this.socket.address();
      console.log(`${new Date().toLocaleString()} Prog ${this.prog} vers ${this.vers} listening on ${address.address}:${address.port}`);
    });

    this.socket.bind(this.udpPort);
  }

  start() {
    Program.programRegistry.registerProgram(this);
    if (typeof this.udpPort !== "undefined" && Program.udpPortsInUse.indexOf(this.udpPort) === -1) {
      Program.udpPortsInUse.push(this.udpPort);
      this.startUdp();
    }
  }
}

module.exports = Program;
