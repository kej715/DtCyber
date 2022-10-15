#!/usr/bin/env node

const Terminal = require("../Terminal");

const term = new Terminal.CybisTerminal();
term.connect(8005)
.then(() => term.say("Connected"))
.then(() => term.say("Login ..."))
.then(() => term.login("guest", "guests", "public"))
.then(() => term.say("Login complete, launch 0solitaire ..."))
.then(() => term.send("0solitaire"))
.then(() => term.sendKey("d", false, true, false))
.then(() => term.expect([ {re:/Press the NEXT key/} ]))
.then(() => term.say("0solitaire running ..."))
.then(() => term.sleep(2000))
.then(() => term.say("Exit 0solitaire ..."))
.then(() => term.sendKey("S", true, true, false))
.then(() => term.expect([ {re:/Choose a lesson/} ]))
.then(() => term.say("Logout ..."))
.then(() => term.sendKey("S", true, true, false))
.then(() => term.expect([ {re:/Press  NEXT  to begin/} ]))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
});
