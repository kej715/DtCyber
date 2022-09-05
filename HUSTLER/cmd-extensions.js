const cmdExtensions = [
  {
    names: ["shutdown"],
    desc:  "'shutdown' shutdown the system gracefully and exit",
    fn:    (dtc, args) => {
      return dtc.dsd([
        "UNL#500#OP #500#",
        "BLITZ.",
        "#2000#STEP."
      ])
      .then(() => dtc.sleep(2000))
      .then(() => dtc.send("shutdown"))
      .then(() => dtc.expect([{ re: /Goodbye for now/ }]))
      .then(() => dtc.say("Shutdown complete"))
      .then(() => {
        process.exit(0);
      });
    }
  }
];

module.exports = cmdExtensions;
