/*
 * This class renders a description of a machine and manages
 * dialogs related to it.
 */

class MachineDescription {

  constructor(machine) {
    this.machine = machine;
  }

  setMachine(machine) {
    this.machine = machine;
  }

  setContainerStack(containerStack) {
    this.containerStack = containerStack;
  }

  getExampleDefinition(id) {
    if (this.machineDefinition.examples) {
      let exampleDefs = this.machineDefinition.examples;
      for (let i = 0; i < exampleDefs.length; i++) {
        let exampleDef = exampleDefs[i];
        if (exampleDef.id === id) return exampleDef;
      }
    }
    return {};
  }

  activateExamples() {
    let me = this;
    let exampleContainer = document.getElementById("example-container");
    if (exampleContainer === null) return;
    const exampleContainerWidth = 640;
    const exampleContainerHeight = 320;
    exampleContainer.style.top = "0px";
    exampleContainer.style.right = "0px";
    exampleContainer.style.width = `${exampleContainerWidth}px`;
    let exampleContentContainer = document.getElementById("example-content-container");
    exampleContentContainer.style.height = `${exampleContainerHeight}px`;
    let busyIndicatorContainer = document.getElementById("busy-indicator-container");
    let busyIndicator = document.getElementById("busy-indicator");
    const exampleCloseBtn = document.getElementById("example-close-button");
    exampleCloseBtn.onclick = e => {
      if (me.timer) {
        clearTimeout(me.timer);
        delete me.timer;
      }
      me.script = [];
      $(exampleContainer).hide();
      $(busyIndicatorContainer).hide();
    };
    busyIndicatorContainer.style.left = `${Math.floor(exampleContainerWidth/2 - busyIndicator.naturalWidth/2)}px`;
    busyIndicatorContainer.style.top = `${Math.floor(exampleContainerHeight/2 - busyIndicator.naturalHeight/2)}px`;
    const dragMgr = new DraggableManager();
    dragMgr.activate(exampleContainer);
    let currentExampleDefinition = {};
    let exampleSegments = [];
    $(".clickable").click(function() {
      let clicked = $(this);
      clicked.toggleClass("selected");
      let id = clicked.attr("id");
      currentExampleDefinition = me.getExampleDefinition(id);
      $("#example-title").text(currentExampleDefinition.name);
      $("#example-content").empty();
      $(exampleContainer).show();
      me.containerStack.promote(exampleContainer);
      $.get(currentExampleDefinition.defn, defn => {
        exampleSegments = defn.split("~~~~\n");
        $("#example-content").html(exampleSegments[0]);
      });
    });
    const runButton = document.getElementById("example-run-button");
    runButton.onclick = e => {
      if (exampleSegments.length > 0) {
        $(busyIndicatorContainer).show();
        me.machine.runScript(exampleSegments[1], () => {
          $(busyIndicatorContainer).hide();
        });
      }
    };
  }

  getMachineDefn(callback) {
    let me = this;
    if (typeof this.machineDefinition !== "undefined") {
      if (typeof callback === "function") callback(this.machineDefinition);
    }
    else {
      $.getJSON(`/machine/${this.machine.getId()}`, machineDefinition => {
        me.machineDefinition = machineDefinition;
        if (typeof callback === "function") callback(me.machineDefinition);
      });
    }
  }

  renderDescription(callback) {
    let me = this;
    this.getMachineDefn(machineDefn => {
      //
      // Render machine overview information
      //
      if (machineDefn.overview) {
        const keyMap = [
          ["model","Make and model:"],
          ["cpus","CPUs"],
          ["pps","PPs"],
          ["wordSize","Word size:"],
          ["addressWidth","Address width:"],
          ["addressSpace","Address space:"],
          ["mips","MIPS rating:"],
          ["os","Operating system:"]
        ];
        const overview = machineDefn.overview;
        let html = '';
        keyMap.forEach(entry => {
          let key = entry[0];
          let label = entry[1];
          if (overview[key]) {
            html += '<div class="machine-overview-property">';
            html += `<div class="machine-overview-label">${label}</div>`;
            html += `<div class="machine-overview-value">${overview[key]}</div>`;
            html += '</div>';
          }
        });
        $("#machine-overview").html(html);
      }
      //
      // Render machine configuration details
      //
      if (machineDefn.configuration) {
        const config = machineDefn.configuration;
        let html = "";
        if (config.processors && config.processors.length > 0) {
          html += '<div class="machine-config-category-entry">';
          html += '<div class="machine-config-category-label">Processors:</div>';
          let n = config.processors.length;
          let details = '';
          config.processors.forEach((processor, i) => {
            let c = processor.count;
            details += `${c} ${processor.type}${c > 1 ? "s" : ""}${(n > 1 && i + 1 < n) ? ", " : ""}`;
          });
          html += `<div class="machine-config-category-details">${details}</div>`;
          html += '</div>';
        }
        if (config.memory && config.memory.length > 0) {
          html += '<div class="machine-config-category-entry">';
          html += '<div class="machine-config-category-label">Memory:</div>';
          let n = config.memory.length;
          let details = '';
          config.memory.forEach((mem, i) => {
            details += `${mem.size} ${mem.units} ${mem.type}${(n > 1 && i + 1 < n) ? ", " : ""}`;
          });
          html += `<div class="machine-config-category-details">${details}</div>`;
          html += '</div>';
        }
        if (config.devices && config.devices.length > 0) {
          html += '<div class="machine-config-category-entry">';
          html += '<div class="machine-config-category-label">Peripherals:</div>';
          let n = config.devices.length;
          let details = '';
          config.devices.forEach((dev, i) => {
            let c = dev.count;
            let capacity = "";
            if (dev.size) {
              capacity = ` (${dev.size} ${dev.units}${c > 1 ? " each" : ""})`;
            }
            details += `${c} ${dev.model} ${dev.type}${c > 1 ? "s" : ""}${capacity}${(n > 1 && i + 1 < n) ? ", " : ""}`;
          });
          html += `<div class="machine-config-category-details">${details}</div>`;
          html += '</div>';
        }
        $("#machine-configuration").html(html);
      }
      //
      // Render machine login instructions
      //
      if (machineDefn.login && machineDefn.login.length > 0) {
        let html = '<div class="machine-login-header">To log into this machine:</div>';
        html += '<div class="machine-login-action">';
        html += '<div class="machine-login-prompt caption">Prompt</div>';
        html += '<div class="machine-login-response caption">Response</div>';
        html += '</div>'
        machineDefn.login.forEach(pr => {
          html += '<div class="machine-login-action">';
          html += `<div class="machine-login-prompt">${pr.prompt}</div>`;
          html += `<div class="machine-login-response">${pr.response}</div>`;
          html += '</div>'
        });
        $("#machine-login").html(html);
      }
      if (machineDefn["login-script"]) {
        let html = 'To log into the guest user automatically, ';
        html += '<a id="machine-login-script-button" class="machine-login-script-button" href="#">click here</a>.';
        $("#machine-login-script-container").html(html);
        $("#machine-login-script-button").click(function() {
          $.get(machineDefn["login-script"], script => {
            me.machine.runScript(script, () => {
              // do nothing for now
            });
          });
        });
      }
      //
      // Render information about examples supported by the machine
      //
      if (machineDefn.examples && machineDefn.examples.length > 0) {
        let html = '<div class="machine-example-header">The examples listed below are supported by this machine.';
        html += ' After logging in, you may click on the name of an example to reveal information about it and run';
        html += ' an automated script showing how it works.</div>';
        html += '<div class="machine-example-name caption">Name</div>';
        html += '<div class="machine-example-ref caption">Description</div>';
        machineDefn.examples.forEach(example => {
          html += '<div class="machine-example-entry">';
          html += `<div id="${example.id}" class="machine-example-name clickable">${example.name}</div>`;
          html += `<div class="machine-example-description">${example.desc}</div>`;
          html += '</div>'
        });
        $("#machine-examples").html(html);
      }
      me.activateExamples();
      //
      // Render information about lessons (e.g., PLATO lessons) supported by the machine
      //
      if (machineDefn.lessons && machineDefn.lessons.length > 0) {
        let html = '<div class="machine-example-header">Some lessons and games available on the system are listed below:</div>';
        html += '<div class="machine-example-name caption">Lesson</div>';
        html += '<div class="machine-example-ref caption">Description</div>';
        machineDefn.lessons.forEach(lesson => {
          html += '<div class="machine-example-entry">';
          html += `<div class="machine-example-name">${lesson.name}</div>`;
          html += `<div class="machine-example-ref">${lesson.description}</div>`;
          html += '</div>'
        });
        $("#machine-lessons").html(html);
      }
      //
      // Render information about keyboard mappings
      //
      if (machineDefn.keys) {
        let html = '<div class="machine-keys-header">Keyboard mappings for interacting with this machine:</div>';
        html += `<div class="machine-keys-effect caption">${machineDefn.keys.labels[0]}</div>`;
        html += `<div class="machine-keys-keystrokes caption">${machineDefn.keys.labels[1]}</div>`;
        machineDefn.keys.mappings.forEach(mapping => {
          html += '<div class="machine-keys-entry">';
          html += `<div class="machine-keys-effect">${mapping[0]}</div>`;
          html += `<div class="machine-keys-keystrokes">${mapping[1]}</div>`;
          html += '</div>'
        });
        $("#machine-keys").html(html);
      }
      //
      // Render link to additional information about the machine
      //
      if (machineDefn.more) {
        $("#machine-more").html(`<a href="${machineDefn.more}" target="_blank">More ...</a>`);
      }

      if (typeof callback === "function") callback();
    });
  }
}
