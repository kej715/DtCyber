#!/usr/bin/env node

const Terminal = require("../Terminal");

const term = new Terminal.PlatoTerminal();

term.connect(5004)
.then(() => term.say("Connected"))
.then(() => term.say("Login ..."))
.then(() => term.login("guest", "guests", "guest"))
.then(() => term.say("Login complete, launch lesson \"aids\" ..."))
.then(() => term.send("aids"))
.then(() => term.sendKey("d", false, true, false))
.then(() => term.expect([ {re:/NEXT  for the main index/} ]))
.then(() => term.say("aids running ..."))
.then(() => term.sleep(2000))
.then(() => term.say("Exit aids ..."))
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
  process.exit(1);
});
