#!/usr/bin/env node

const DEBUG = false;

const DtCyber   = require("../../automation/DtCyber");
const Terminal  = require("../../automation/Terminal");
const utilities = require("./utilities");

const dtc         = new DtCyber();
const term        = new Terminal.AnsiTerminal();
const customProps = utilities.getCustomProperties(dtc);

const ipAddress = dtc.getHostIpAddress();
term.connect(`${ipAddress}:23`)
.then(() => term.say("Connected"))
.then(() => term.say("Login ..."))
.then(() => term.loginNOS2("", "INSTALL", utilities.getPropertyValue(customProps, "PASSWORDS", "INSTALL", "INSTALL")))
.then(() => term.say("logged in"))
.then(() => term.send("GET,NAMSTRT/UN=NETOPS\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.say("Got NAMSTRT"))
.then(() => term.say("Edit NAMSTRT ..."))
.then(() => term.send("XEDIT,NAMSTRT\r"))
.then(() => term.expect([{ re: /\?\? / }]))
.then(() => term.send("L/USER(WWW,/\r"))
.then(() => term.expect([{ re: /\?\? / }]))
.then(() => term.say("Located USER(WWW ..."))
.then(() => term.send("D\r"))
.then(() => term.expect([{ re: /\?\? / }]))
.then(() => term.send("IB1\r"))
.then(() => term.expect([{ re: /\? / }]))
.then(() => term.send(`USER(WWW,${utilities.getPropertyValue(customProps,"PASSWORDS","WWW","WWWX")})\r`))
.then(() => term.expect([{ re: /\?\? / }]))
.then(() => term.say("Updated password of WWW ..."))
.then(() => term.say("Exit editor ..."))
.then(() => term.send("Q\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.send("REPLACE,NAMSTRT/UN=NETOPS\r"))
.then(() => term.expect([{ re: /\// }]))
.then(() => term.say("Replaced NAMSTRT"))
.then(() => term.send("LOGOUT\r"))
.then(() => term.expect([{ re: /CHARACTERS=/ }]))
.then(() => term.say("Logged out"))
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
