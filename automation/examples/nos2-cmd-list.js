#!/usr/bin/env node

const fs       = require("fs");
const Terminal = require("../Terminal");

const cmds = [
  "catlist\r",
  "status,f\r"
];

const term = new Terminal.AnsiTerminal();
term.setTracer();
term.connect(23)
.then(() => term.say("Connected"))
.then(() => term.say("Logging in ..."))
.then(() => term.loginNOS2("", "guest", "guest"))
.then(() => term.say("Login complete, execute command list ..."))
.then(() => term.sleep(500))
.then(() => term.sendList(cmds, /\//, 250))
.then(() => term.send("logout\r"))
.then(() => term.expect([{ re:/LOGGED OUT/ }]))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
