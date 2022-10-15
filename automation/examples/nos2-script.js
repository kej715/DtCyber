#!/usr/bin/env node

const fs       = require("fs");
const Terminal = require("../Terminal");

const term = new Terminal.AnsiTerminal();
term.setTracer();
term.connect(23)
.then(() => term.say("Connected"))
.then(() => term.say("Login ..."))
.then(() => term.loginNOS2("", "guest", "guest"))
.then(() => term.say("Login complete, run script ..."))
.then(() => term.sleep(500))
.then(() => term.runScript(fs.readFileSync("ftn5.txt", "utf8")))
.then(() => term.send("logout\r"))
.then(() => term.expect([{ re:/LOGGED OUT/ }]))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
});
