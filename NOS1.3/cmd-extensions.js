const cmdExtensions = [
  {
    names: ["make_ds_tape", "mdt"],
    desc:  "'make_ds_tape' make a new deadstart tape",
    fn:    (dtc, args) => {
      return dtc.say("Mount new deadstart tape")
      .then(() => dtc.dsd([
        "[UNLOAD,51.",
        "[!"
      ]))
      .then(() => dtc.sleep(2000))
      .then(() => dtc.mount(13, 0, 1, "tapes/newds.tap", true))
      .then(() => dtc.sleep(5000))
      .then(() => dtc.say("Run job to write new deadstart tape"))
      .then(() => dtc.runJob(11, 4, "decks/make-ds-tape.job", [51]))
      .then(() => dtc.say("New deadstart tape created: tapes/newds.tap"));
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
  }
];

module.exports = cmdExtensions;
