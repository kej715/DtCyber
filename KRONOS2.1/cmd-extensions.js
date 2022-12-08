const cmdExtensions = [
  {
    names: ["shutdown"],
    desc:  "'shutdown' shutdown the system gracefully and exit",
    fn:    (dtc, args) => {
      return dtc.say("Starting shutdown sequence ...")
      .then(() => dtc.dsd("[UNLOCK."))
      .then(() => dtc.dsd("[BLITZ."))
      .then(() => dtc.sleep(5000))
      .then(() => dtc.dsd("CHECK#2000#"))
      .then(() => dtc.sleep(2000))
      .then(() => dtc.dsd("STEP."))
      .then(() => dtc.sleep(1000))
      .then(() => dtc.send("shutdown"))
      .then(() => dtc.expect([{ re: /Goodbye for now/ }]))
      .then(() => dtc.say("Shutdown complete ..."))
      .then(() => {
        process.exit(0);
      });
    }
  }
];

module.exports = cmdExtensions;
