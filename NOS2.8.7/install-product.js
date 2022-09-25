#!/usr/bin/env node

const child_process = require("child_process");
const fs = require("fs");
const DtCyber = require("../automation/DtCyber");

const baseProducts = ["atf","iaf","nam5","nos","tcph"];
let installedProductSet = [];
let productSet = [];

const addDependencies = (prodDefn, isSysedit) => {
  if (typeof prodDefn.depends !== "undefined") {
    const productName = prodDefn.name;
    for (const dependencyName of prodDefn.depends) {
      if (isInstalled(dependencyName) === false
          || baseProducts.indexOf(dependencyName) !== -1) {
        const dependencyDefn = lookupProduct(dependencyName);
        if (dependencyDefn === null) {
          process.stderr.write(`${productName} depends upon ${dependencyName}, but it does not exist.\n`);
          process.exit(1);
        }
        if (addProduct(dependencyDefn, isSysedit)) {
          process.stdout.write(`${new Date().toLocaleTimeString()} ${dependencyName} will be installed because ${productName} depends upon it.\n`);
        }
      }
    }
  }
};

const addInstalledProduct = name => {
  if (installedProductSet.indexOf(name) === -1)
    installedProductSet.push(name);
};

const addProduct = (prodDefn, isSysedit) => {
  for (const pe of productSet) {
    if (prodDefn.name === pe.prodDefn.name) return false;
  }
  addDependencies(prodDefn, isSysedit);
  productSet.push({
    prodDefn:prodDefn,
    isSysedit:isSysedit
  });
  return true;
};

const installProduct = productEntry => {

  const prodDefn = productEntry.prodDefn;
  const productName = prodDefn.name;
  const isForcedInstall = productEntry.isForcedInstall;
  const isSysedit = productEntry.isSysedit;

  let promise = dtc.say("----------------------------------")
  .then(() => dtc.say(`Starting installation of ${productName}`));

  let filename = null;
  let filetype = null;
  let progressMaxLen = 0;
  
  if (typeof prodDefn.url !== "undefined") {
    filename = prodDefn.url.split("/").pop().split("?")[0];
    filetype = filename.split(".").pop();
    promise = promise
    .then(() => dtc.say("Download tape image ..."))
    .then(() => dtc.wget(prodDefn.url, "opt/tapes", filename, (byteCount, contentLength) => {
      let progress = `\r${new Date().toLocaleTimeString()}   Received ${byteCount}`;
      if (contentLength === -1) {
        progress += " bytes";
      }
      else {
        progress += ` of ${contentLength} bytes (${Math.round((byteCount / contentLength) * 100)}%)`;
      }
      if (progress.length > progressMaxLen) progressMaxLen = progress.length;
      process.stdout.write(progress)
    }))
    .then(() => new Promise((resolve, reject) => {
      let progress = `\r`;
      while (progress.length++ < progressMaxLen) progress += " ";
      process.stdout.write(`${progress}\r`);
      resolve();
    }));
    if (filetype === "tap") {
      promise = promise
      .then(() => dtc.say("Mount tape ..."))
      .then(() => dtc.dsd([
        "[UNLOAD,51.",
        "[!"
      ]))
      .then(() => dtc.mount(13, 0, 1, `opt/tapes/${filename}`))
      .then(() => dtc.sleep(5000));
    }
    else if (filetype === "zip") {
      promise = promise
      .then(() => dtc.say("Extract contents of ZIP file ..."))
      .then(() => dtc.unzip(`opt/tapes/${filename}`, "opt/tapes"));
    }
  }
  
  if (typeof prodDefn.pre !== "undefined") {
    promise = promise
    .then(() => dtc.say(`Run ${productName} PRE-install tasks ...`))
    .then(() => runWorkList(dtc, prodDefn.pre, "opt"));
  }
  
  const jobPath = `opt/${productName}.job`;
  if (fs.existsSync(jobPath)) {
    promise = promise
    .then(() => dtc.say(`Run job ${jobPath} ...`))
    .then(() => dtc.runJob(12, 4, jobPath, [prodDefn.lib, 51]));
  }
  
  if (typeof prodDefn.post !== "undefined") {
    promise = promise
    .then(() => dtc.say(`Run ${productName} POST-install tasks ...`))
    .then(() => runWorkList(dtc, prodDefn.post, "opt"));
  }
  
  if (typeof prodDefn.url !== "undefined" && filetype === "tap") {
    promise = promise
    .then(() => dtc.dsd([
      "[UNLOAD,51.",
      "[!"
    ]));
  }
  
  if (isSysedit || typeof prodDefn.examples !== "undefined") {
    if (typeof prodDefn.records !== "undefined") {
      let cmds = [];
      let recs = prodDefn.records;
      if (recs.indexOf("$") >= 0) {
        cmds.push("UCC=+.");
        recs = recs.split("$").join("+S");
      }
      cmds.push(
        "ATTACH,P=PRODUCT.",
        `GTR,P,LGO,U.${recs}`,
        "#12000#SYSEDIT,B=LGO."
      );
      promise = promise
      .then(() => dtc.dsd([
        "[IDLE,IAF.",
        "#1000#[!"
      ]))
      .then(() => dtc.dis(cmds, 1))
      .then(() => dtc.sleep(30000))
      .then(() => dtc.dsd("[IAF."));
    }
    if (typeof prodDefn.examples !== "undefined") {
      promise = promise
      .then(() => dtc.say(`Run ${productName} example installation tasks ...`))
      .then(() => runWorkList(dtc, prodDefn.examples, "examples"));
    }
  }
  
  return promise.then(() => dtc.say(`${productName} installed successfully`));
}

const isInstalled = name => {
  return installedProductSet.indexOf(name) !== -1
         || baseProducts.indexOf(name) !== -1;
};

const listProducts = () => {
  for (const category of products) {
    process.stdout.write("\n------------------------------------------------------------\n");
    process.stdout.write(`Category: ${category.category}\n`);
    let words = category.desc.split(" ");
    let line = "";
    while (words.length > 0) {
      let word = words.shift();
      if (line.length + word.length < 60) {
        line += `${word} `;
      }
      else {
        process.stdout.write(line);
        process.stdout.write("\n");
        line = `${word} `;
      }
    }
    process.stdout.write(line);
    process.stdout.write("\n");
    process.stdout.write("------------------------------------------------------------\n");
    for (const prodDefn of category.products) {
      let name = prodDefn.name;
      process.stdout.write(name);
      for (let i = name.length; i < 10; i++) process.stdout.write(" ");
      process.stdout.write(` : ${prodDefn.desc}\n`);
    }
  }
};

const lookupProduct = name => {
  for (const category of products) {
    for (const prodDefn of category.products) {
      if (prodDefn.name === name) return prodDefn;
    }
  }
  return null;
};

const runWorkList = (dtc, workList, dir) => {
  if (typeof workList === "string") workList = [workList];
  let promise = Promise.resolve();
  for (const item of workList) {
    if (item.endsWith(".job")) {
      promise = promise
        .then(() => dtc.say(`  Run job ${dir}/${item} ...`))
        .then(() => dtc.runJob(12, 4, `${dir}/${item}`));
    }
    else {
      promise = promise
      .then(() => dtc.say(`  Run command "node ${dir}/${item}" ...`))
      .then(() => dtc.disconnect())
      .then(() => dtc.exec("node", [`${dir}/${item}`]))
      .then(() => dtc.connect())
      .then(() => dtc.expect([ {re:/Operator> $/} ]));
    }
  }
  return promise;
};

const usage = status => {
  process.stderr.write("Usage: node install-product [-f][+f][-s][+s] <name> [[-f][+f][-s][+s] <name> ...]\n");
  process.stderr.write("      -f   force re/installation of subsequently named products\n");
  process.stderr.write("      +f   do not install subsequently named products if they're already installed\n");
  process.stderr.write("      -s   SYSEDIT subsequently named products into the running system\n");
  process.stderr.write("      +s   do not SYSEDIT subsequently named products into the running system\n");
  process.stderr.write("  <name>   name of product or category to install\n");
  process.stderr.write("           Category names:\n");
  for (const category of products) {
    let name = category.category;
    let desc = (typeof category.shortDesc === "undefined") ? category.desc : category.shortDesc;
    process.stderr.write("             ");
    for (let len = name.length; len < 8; len++) process.stderr.write(" ");
    process.stderr.write(`${name} : ${desc}\n`);
  }
  process.stderr.write("           Special names:\n");
  process.stderr.write("                  all : Install all products\n");
  process.stderr.write("                 list : List all available products\n");
  process.exit(status);
};

const products = JSON.parse(fs.readFileSync("opt/products.json", "utf8"));
if (fs.existsSync("opt/installed.json")) {
  installedProductSet = JSON.parse(fs.readFileSync("opt/installed.json", "utf8"));
}
let isForcedInstall = false;
let isSysedit = false;

let categories = {};
let categoryNames = [];
for (const category of products) {
  const name = category.category;
  categoryNames.push(name);
  categories[name] = category.products;
}

for (let i = 2; i < process.argv.length; i++) {
  let arg = process.argv[i];
  switch (arg) {
  case "-f":
    isForcedInstall = true;
    break;
  case "+f":
    isForcedInstall = false;
    break;
  case "-h":
  case "-?":
  case "-help":
  case "--help":
    usage(0);
    break;
  case "-s":
    isSysedit = true;
    break;
  case "+s":
    isSysedit = true;
    break;
  case "all":
    for (const category of products) {
      for (const prodDefn of category.products) {
        if ((isForcedInstall && baseProducts.indexOf(prodDefn.name) == -1)
          || isInstalled(prodDefn.name) === false) {
          addProduct(prodDefn, isSysedit);
        }
      }
    }
    break;
  case "help":
    usage(0);
    break;
  case "list":
    listProducts();
    process.exit(0);
    break;
  default:
    if (arg.startsWith("-") || arg.startsWith("+")) {
      process.stderr.write(`Unrecognized option: ${arg}\n`);
      usage(1);
    }
    let productName = arg.toLowerCase();
    if (categoryNames.indexOf(productName) >= 0) {
      for (const prodDefn of categories[productName]) {
        if ((isForcedInstall && baseProducts.indexOf(prodDefn.name) == -1)
          || isInstalled(prodDefn.name) === false) {
          addProduct(prodDefn, isSysedit);
        }
      }
    }
    else {
      let prodDefn = lookupProduct(productName);
      if (prodDefn === null) {
        process.stderr.write(`Unrecognized product name: ${arg}\n`);
        process.exit(1);
      }
      if (isForcedInstall || isInstalled(productName) === false) {
        addProduct(prodDefn, isSysedit);
      }
      else {
        process.stdout.write(`${process.argv[i]} is already installed.\n`);
      }
    }
    break;
  }
}

if (productSet.length < 1) {
  process.stderr.write("No products need to be installed.\n");
  process.exit(0);
}

const dtc = new DtCyber();

let promise = dtc.connect()
.then(() => dtc.expect([ {re:/Operator> $/} ]))
.then(() => dtc.console("idle off"))
.then(() => dtc.attachPrinter("LP5xx_C12_E5"));
for (const productEntry of productSet) {
  promise = promise
  .then(() => installProduct(productEntry))
  .then(() => {
    addInstalledProduct(productEntry.prodDefn.name);
    fs.writeFileSync("opt/installed.json", JSON.stringify(installedProductSet));
    return Promise.resolve();
  });
}
promise = promise
.then(() => dtc.console("idle on"));

if (productSet.length > 1) {
  promise = promise
  .then(() => dtc.say("All requested products installed"));
}
promise
.then(() => {
  process.exit(0);
})
.catch(err => {
  process.stderr.write(`${err}\n`);
  process.exit(1);
});
