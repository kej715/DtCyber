#!/usr/bin/env node

const Terminal = require("../Terminal");

const term = new Terminal.PlatoTerminal();

term.connect(5004)
.then(() => term.say("Connected"))
.then(() => term.say("Login ..."))
.then(() => term.login("guest", "guests", "guest"))
.then(() => term.say("Done."))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
});
