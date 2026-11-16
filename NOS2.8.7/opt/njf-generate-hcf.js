#!/usr/bin/env node
//
// Generate the source for the NJF Host Configuration File.
//

const DtCyber   = require("../../automation/DtCyber");
const fs        = require("fs");
const utilities = require("./utilities");

const dtc = new DtCyber();

const customProps = utilities.getCustomProperties(dtc);
const mid         = utilities.getMachineId(dtc);
const hostID      = utilities.getHostId(dtc);
const njeTopology = utilities.getNjeTopology(dtc);
const rhpTopology = utilities.getRhpTopology(dtc);

const toHex = value => {
  return (value < 16 ? `0${value.toString(16)}` : value.toString(16)).toUpperCase();
};

let hcfSource  = [];
let lineNumber = 1;
let nodeNumber = 1;

//  1. Generate local system's node definition
hcfSource = hcfSource.concat([
  "*",
  "*  LOCAL NJE NODE",
  "*",
  `NODE,${nodeNumber},${hostID},M${mid},,IBM ,NET,JOB.`,
  `OWNNODE,${nodeNumber},CYBER,NETOPS.`
]);
nodeNumber += 1;

//  2. Generate definitions of adjacent nodes
hcfSource = hcfSource.concat([
  "*",
  "*  ADJACENT NJE NODES",
  "*"
]);
let rhpLidMap = {};
for (const node of rhpTopology) {
  rhpLidMap[node.id] = node.lid;
}
let names = Object.keys(utilities.getAdjacentNjeNodes(dtc)).sort();
for (const name of names) {
  let node = njeTopology[name];
  if (typeof node.lid === "undefined" || node.lid === "" || node.lid === null) {
    throw new Error(`Adjcent node ${node.id} does not have a LID`);
  }
  let lid = node.lid;
  if (typeof rhpLidMap[name] !== "undefined" && rhpLidMap[name] === lid) {
    lid = `N${lid.substring(1)}`;
  }
  node.nodeNumber = nodeNumber++;
  hcfSource.push(
    `NODE,${node.nodeNumber},${node.id},${lid},,IBM ,NET,JOB.`
  );
}

//  3. Generate definitions for non-adjacent nodes with LIDs
names = Object.keys(utilities.getNonadjacentNjeNodesWithLids(dtc)).sort();
if (names.length > 0) {
  hcfSource = hcfSource.concat([
    "*",
    "*  NON-ADJACENT NJE NODES WITH LIDS",
    "*"
  ]);
  for (const name of names) {
    let node = njeTopology[name];
    node.nodeNumber = nodeNumber++;
    let lid = node.lid;
    if (typeof rhpLidMap[name] !== "undefined" && rhpLidMap[name] === lid) {
      lid = `N${lid.substring(1)}`;
    }
    hcfSource.push(
      `NODE,${node.nodeNumber},${node.id},${lid},,IBM ,NET,JOB.`
    );
  }
  hcfSource = hcfSource.concat([
    "*",
    "*  PATHS TO NON-ADJACENT NJE NODES WITH LIDS",
    "*"
  ]);
  for (const name of names) {
    let node = njeTopology[name];
    node.nodeNumber = nodeNumber++;
    hcfSource.push(
      `PATH,${node.id},${node.route}.`
    );
  }
}

//  4. Generate line definitions
hcfSource = hcfSource.concat([
  "*",
  "*  NETWORK LINE DEFINITIONS",
  "*"
]);
names = Object.keys(utilities.getAdjacentNjeNodes(dtc)).sort();
for (const name of names) {
  let node = njeTopology[name];
  if (typeof node.netDevice !== "undefined") {
    node.lineNumber = lineNumber++;
    hcfSource.push(
      `LNE,${node.lineNumber},TE${toHex(node.netDevice.npuNode)}P${toHex(node.firstClaPort)},1000.`
    );
  }
}

//  5. Generate auto-start line definitions
hcfSource = hcfSource.concat([
  "*",
  "*  AUTO-START DIRECTIVES",
  "*"
]);
for (const name of names) {
  let node = njeTopology[name];
  if (typeof node.netDevice !== "undefined") {
    hcfSource.push(
      `ASLNE,${node.lineNumber},ST1,SR1${node.software === "NJEF" ? ",JT1,JR1" : ""}.`
    );
  }
}

//  6. Generate FAMILY statements for routing to non-adjacent nodes without LIDs.
//     Associate each non-adjacent node with the closest adjacent node through which
//     the non-adjacent node may be reached.
names = Object.keys(utilities.getNonadjacentNjeNodesWithoutLids(dtc)).sort();
if (names.length > 0) {
  hcfSource = hcfSource.concat([
    "*",
    "*  ROUTES TO NON-ADJACENT NODES WITHOUT LIDS",
    "*"
  ]);
  let n = 1;
  for (const name of names) {
    let node = njeTopology[name];
    let route = (typeof node.route !== "undefined" && node.route !== null) ? node.route : utilities.getDefaultNjeRoute(dtc);
    let lid = njeTopology[route].lid;
    if (typeof rhpLidMap[route] !== "undefined" && rhpLidMap[route] === lid) {
      lid = `N${lid.substring(1)}`;
    }
    hcfSource.push(
      `FAMILY,${node.id},NET${n++},${lid},${route}.`
    );
  }
}
hcfSource.push("");

dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"))
.then(() => dtc.say("Create/update NJF host configuration file ..."))
.then(() => {
  let job = [
    "$COPY,INPUT,HCFSRC.",
    "$REPLACE,HCFSRC.",
    "$PERMIT,HCFSRC,INSTALL=W."
  ];
  const options = {
    jobname:  "HCFSRC",
    username: "NETADMN",
    password: utilities.getPropertyValue(customProps, "PASSWORDS", "NETADMN", "NETADMN"),
    data:     hcfSource.join("\n")
  };
  return dtc.createJobWithOutput(12, 4, job, options);
})
.then(() => {
  process.exit(0);
})
.catch(err => {
  console.log(err);
  process.exit(1);
});
