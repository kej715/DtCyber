#!/usr/bin/env node

const fs       = require("fs");
const Terminal = require("../Terminal");

const term = new Terminal.AnsiTerminal();
term.setTracer();
term.connect(6676)
.then(() => term.say("Connected"))
.then(() => term.say("Logging in ..."))
.then(() => term.loginNOS1("guest", "guest"))
.then(() => term.say("Login complete, run script ..."))
.then(() => term.sleep(500))
.then(() => term.runScript(fs.readFileSync("ftn4.txt", "utf8")))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
