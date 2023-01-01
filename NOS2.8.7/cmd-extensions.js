const cmdExtensions = [
  {
    names: ["activate_tms"],
    desc:  "'activate_tms' activate the NOS Tape Management System",
    fn:    (dtc, args) => {
      return dtc.disconnect()
      .then(() => dtc.exec("node", ["activate-tms"]))
      .then(() => dtc.connect())
      .then(() => dtc.expect([ {re:/Operator> $/} ]));
    }
  },
  {
    names: ["install_product", "install", "ip"],
    desc:  "'install_product [-f] name...' install optional product(s)",
    fn:    (dtc, args) => {
      return dtc.disconnect()
      .then(() => dtc.exec("node", ["install-product"].concat(args.split(" "))))
      .then(() => dtc.connect())
      .then(() => dtc.expect([ {re:/Operator> $/} ]));
    }
  },
  {
    names: ["make_ds_tape", "mdt"],
    desc:  "'make_ds_tape' make a new deadstart tape",
    fn:    (dtc, args) => {
      return dtc.say("Load tapes ...")
      .then(() => dtc.dsd([
        "[UNLOAD,50.",
        "[UNLOAD,51.",
        "[!"
      ]))
      .then(() => dtc.mount(13, 0, 0, "tapes/ds.tap"))
      .then(() => dtc.mount(13, 0, 1, "tapes/newds.tap", true))
      .then(() => dtc.sleep(5000))
      .then(() => dtc.say("Run job to write new deadstart tape ..."))
      .then(() => dtc.runJob(12, 4, "decks/make-ds-tape.job", [50, 51]))
      .then(() => dtc.say("New deadstart tape created: tapes/newds.tap"));
    }
  },
  {
    names: ["reconfigure", "rcfg"],
    desc:  "'reconfigure [pathname...]' apply site-defined configuration parameters",
    fn:    (dtc, args) => {
      return dtc.disconnect()
      .then(() => dtc.exec("node",
        typeof args !== "undefined" && args.length > 0
        ? ["reconfigure"].concat(args.split(" "))
        : ["reconfigure"]))
      .then(() => dtc.connect())
      .then(() => dtc.expect([ {re:/Operator> $/} ]));
    }
  },
  {
    names: ["shutdown"],
    desc:  "'shutdown' shutdown the system gracefully and exit",
    fn:    (dtc, args) => {
      return dtc.shutdown()
      .then(() => {
        process.exit(0);
      });
    }
  },
  {
    names: ["sync_tms"],
    desc:  "'sync_tms' synchronize the NOS Tape Management System catalog with the Stk simulator configuration",
    fn:    (dtc, args) => {
      return dtc.disconnect()
      .then(() => dtc.exec("node", ["sync-tms"]))
      .then(() => dtc.connect())
      .then(() => dtc.expect([ {re:/Operator> $/} ]));
    }
  }
];

module.exports = cmdExtensions;
