#!/usr/bin/env node

const Terminal = require("../Terminal");

const term = new Terminal.CybisTerminal();
term.connect(8005)
.then(() => term.say("Connected"))
.then(() => term.say("Login ..."))
.then(() => term.login("guest", "guests", "public"))
.then(() => term.say("Done."))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
});
