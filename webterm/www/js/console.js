/*--------------------------------------------------------------------------
**
**  Copyright (c) 2023, Kevin Jordan
**
**  console.js
**    This module provides classes that emulate the Cyber console.
**
**--------------------------------------------------------------------------
*/

/*
 *  CyberConsole
 *
 *  This class emulates the Cyber console in 2d space.
 *
 *  It uses two drawing canvasses and associated contexts:
 *  1. The "on screen" canvas/context which is mapped to a visible HTML canvas.
 *     This uses a bitmaprenderer context and is not drawn into directly.
 *  2. The "off screen" canvas/context into which things are drawn using standard
 *     HTML 5 functions.
 *  Every 100ms, a bitmap is created from "off screen" context and transferred
 *  to the "on screen" context.
 */

class CyberConsole extends CyberConsoleBase {

  constructor() {

    super();

    //
    // Console screen offsets
    //
    this.SCREEN_GAP      = 40;
    this.SCREEN_MARGIN   = 20;
    //
    // Console emulation properties
    //
    this.bgndColor         = "black";
    this.currentFont       = this.SMALL_FONT;
    this.fgndColor         = "lightgreen";
    this.fontFamily        = "Lucida Typewriter";
    this.fontHeights       = [0, 10, 20, 40];
    this.offscreenCanvas   = null;
    this.offscreenContext  = null;
    this.onscreenCanvas    = null;
    this.state             = this.ST_TEXT;
    this.x                 = 0;
    this.y                 = 0;
    this.yPointOffset      = Math.round(this.SCREEN_MARGIN / 2);

    //
    // Calculate font information
    //
    this.fonts      = [""];
    this.fontWidths = [2];
    let testLine = "MMMMMMMMMMMMMMMM" // 64 M's
                 + "MMMMMMMMMMMMMMMM"
                 + "MMMMMMMMMMMMMMMM"
                 + "MMMMMMMMMMMMMMMM";
    let cnv = document.createElement("canvas");
    cnv.width = testLine.length * 16;
    cnv.height = this.fontHeights[this.SMALL_FONT];
    let ctx = cnv.getContext("2d");
    let font = `${this.fontHeights[this.SMALL_FONT]}px ${this.fontFamily}`;
    this.fonts.push(font);
    ctx.font = font;
    let fontWidth = Math.round(ctx.measureText(testLine).width / testLine.length);
    this.xRatio = fontWidth / 8;
    this.yRatio = this.fontHeights[this.SMALL_FONT] / 8;
    this.fontWidths.push(fontWidth);
    for (const fontSize of [this.MEDIUM_FONT, this.LARGE_FONT]) {
      this.fonts.push(`${this.fontHeights[fontSize]}px ${this.fontFamily}`);
      fontWidth *= 2;
      this.fontWidths.push(fontWidth);
    }
    this.screenWidth  = this.fontWidths[this.SMALL_FONT] * 64;
    this.screenHeight = this.fontHeights[this.SMALL_FONT] * 64;
    this.canvasWidth  = (this.screenWidth * 2) + this.SCREEN_GAP + (this.SCREEN_MARGIN * 2);
    this.canvasHeight = this.screenHeight + (this.SCREEN_MARGIN * 2);
  }

  setFont(font) {
    this.currentFont = font;
    this.offscreenContext.font = this.fonts[font];
  }

  setScreen(screenNumber) {
    this.xOffset = (b === 1) ? this.screenWidth + this.SCREEN_GAP + this.SCREEN_MARGIN : this.SCREEN_MARGIN;
  }

  setUplineDataSender(callback) {
    this.uplineDataSender = callback;
  }

  getCanvas() {
    return this.onscreenCanvas;
  }

  getContext() {
    return this.onscreenCanvas.getContext("bitmaprenderer");
  }

  getWidth() {
    return this.onscreenCanvas.width;
  }

  processKeyboardEvent(keyStr, shiftKey, ctrlKey, altKey) {
    let sendStr = "";
    //
    // Handle non-special keys
    //
    if (keyStr.length < 2) { // non-special key
      if (ctrlKey === false && altKey === false) {
        sendStr = keyStr;
      }
    }
    //
    // Handle special keys
    //
    else {

      switch (keyStr) {
      case "Backspace":
      case "Delete":
        sendStr = "\b";
        break;
      case "Enter":
        sendStr = "\r";
        break;
      default: // ignore the key
        break;
      }
    }
    if (sendStr.length > 0 && this.uplineDataSender) {
      this.uplineDataSender(sendStr);
    }
  }

  /*
   *  createScreen
   *
   *  Create the on and off screen canvasses and associated contexts, and 
   *  establish event listeners.
   */
  createScreen() {
    this.onscreenCanvas = document.createElement("canvas");
    this.onscreenContext = this.getContext();
    this.onscreenCanvas.width = this.canvasWidth;
    this.onscreenCanvas.height = this.canvasHeight;
    this.onscreenCanvas.style.cursor = "default";
    this.onscreenCanvas.style.border = "1px solid black";
    this.onscreenCanvas.setAttribute("tabindex", 0);
    this.onscreenCanvas.setAttribute("contenteditable", "true");
    this.offscreenCanvas = new OffscreenCanvas(this.canvasWidth, this.canvasHeight);
    this.offscreenContext = this.offscreenCanvas.getContext("2d");
    this.offscreenContext.textBaseline = "bottom";
    this.reset();
    this.clearScreen();

    const me = this;

    this.onscreenCanvas.addEventListener("click", () => {
      me.onscreenCanvas.focus();
    });

    this.onscreenCanvas.addEventListener("mouseover", () => {
      me.onscreenCanvas.focus();
    });

    this.onscreenCanvas.addEventListener("keydown", evt => {
      evt.preventDefault();
      me.processKeyboardEvent(evt.key, evt.shiftKey, evt.ctrlKey, evt.altKey);
    });
  }

  clearScreen() {
    this.offscreenContext.fillStyle = this.bgndColor;
    this.offscreenContext.clearRect(0, 0, this.canvasWidth, this.canvasHeight);
    this.offscreenContext.fillRect(0, 0, this.canvasWidth, this.canvasHeight);
    this.offscreenContext.fillStyle = this.fgndColor;
  }

  reset() {
    this.setFont(this.SMALL_FONT);
    this.offscreenContext.fillStyle = this.fgndColor;
    this.state        = this.ST_TEXT;
    this.x            = 0;
    this.xOffset      = this.SCREEN_MARGIN;
    this.y            = 0;
  }

  displayNotification(font, x, y, s) {
    this.setFont(font);
    this.x           = x;
    this.xOffset     = this.SCREEN_MARGIN;
    this.y           = y;
    this.state       = this.ST_TEXT;
    this.clearScreen();
    for (const line of s.split("\n")) {
      this.renderText(line);
      this.x  = x;
      this.y += this.fontHeights[this.currentFont];
    }
    this.updateScreen();
  }

  drawChar(b) {
    this.offscreenContext.fillText(String.fromCharCode(b), this.x + this.xOffset, this.y + this.SCREEN_MARGIN);
  }

  drawPoint() {
    let x = this.x + this.xOffset;
    let y = this.y + this.yPointOffset;
    this.offscreenContext.clearRect(x, y, 2, 2);
    this.offscreenContext.fillRect(x, y, 2, 2);
  }

  updateScreen() {
    this.onscreenContext.transferFromImageBitmap(this.offscreenCanvas.transferToImageBitmap());
    this.clearScreen();
  }
}

/*
 *  CyberConsole3D
 *
 *  This class emulates the Cyber console in 3d space. It uses Babylon.js to
 *  implement 3d graphics using two drawing canvasses and associated contexts:
 *  1. The "on screen" canvas/context which can be used by the Babylon.js render engine.
 *     This uses a WebGL context and is not drawn into directly.
 *  2. The "off screen" canvas/context into which things are drawn using standard
 *     HTML 5 functions.
 *  At the end of each frame received from DtCyber, the "off screen" context is
 *  transferred to a texture mapped on a 3D plane, which is rendered by Babylon.js.
 */

class CyberConsole3D extends CyberConsole {

  //
  // Console types
  //
  static CC545       = 0;
  static CO6602      = 1;

  constructor(babylon) {
    super();
    this.babylon     = babylon;
    this.consoleType = CyberConsole3D.CC545;
    this.engine      = null;
    this.environMap  = "room-1.env";
    this.glbFileName = "a-opt-cc545.glb";
    this.scene       = null;
  }

  setConsoleType(type) {
    switch (type) {
    case CyberConsole3D.CC545:
      this.glbFileName = "a-opt-cc545.glb";
      break;
    case CyberConsole3D.CO6602:
      this.glbFileName = "a-opt-co6602.glb";
      break;
    default:
      throw new Error(`Unknown console type: ${type}`);
    }
    this.consoleType = type;
  }

  createScreen() {

    super.createScreen();

    const me = this;

    // Initialise Babylon.js mechansims.
    const initFunction = async function() {

      const asyncCreateEngine = async function() {
        return new me.babylon.Engine(me.onscreenCanvas, true, { preserveDrawingBuffer: true, stencil: true,  disableWebGL2Support: false});
      };

      // Wait for the rendering engine to become available, then create the scene.
      me.engine = await asyncCreateEngine();
      me.scene = me.createScene(); // create the scene that will be rendered
    };

    // After initialisation, start the rendering loop.
    initFunction().then(() => {
      me.engine.runRenderLoop(() => {
        if (me.scene && me.scene.activeCamera) {  // If we have a scene and a camera, render the scene.
          me.scene.render();
        }
      });
    });
  }

  /*
   *  createScene - Create the scene (what we want Babylon.js to render).
   */
  createScene () {
    const me = this;
    let scene = new this.babylon.Scene(this.engine);

    // Try to need less coffee when double click selecting.
    this.babylon.Scene.DoubleClickDelay = 500; // ms

    // This is required for compatibility with objects created in Blender.
    // Also seems to turn on Euler rotations instead of using Quaternions.
    scene.useRightHandedSystem = true;

    // Apply post-rendering image processing tweaks. Do not use for now.
    //scene.imageProcessingConfiguration.contrast = 1.1;
    //scene.imageProcessingConfiguration.exposure = 0.8;
    //scene.imageProcessingConfiguration.toneMappingEnabled = false;

    // Add a camera. Let the pointer control the camera.
    let camera = new this.babylon.ArcRotateCamera("camera", -Math.PI / 2, Math.PI / 2.5, 10, new this.babylon.Vector3(0, 0, 0), scene);
    camera.minZ = 0.04; // Clip planes for better Z behaviour.
    camera.maxZ = 10.0;
    camera.fov  = 0.4;  // Radians = about 23 degrees ~ 80mm on 35mm camera. Default 0.8 is too wide for this.
    camera.attachControl(this.onscreenCanvas, true);

    // Add lighting.
    let light = new this.babylon.HemisphericLight("light", new this.babylon.Vector3(1.5, 2, 3), scene);
    scene.environmentTexture = new this.babylon.CubeTexture(`3d/${this.environMap}`, scene);  // Needs more thought!

    // Create a dynamic texture incorporating the HTML canvas.
    this.texture = new this.babylon.DynamicTexture("dynamicTexture", this.offscreenCanvas, scene, false);

    // Create a material with an emissive texture set to the dynamic texture.
    // Adjust texture coordinates to center the display.
    let material = new this.babylon.PBRMaterial("material", scene);
    material.emissiveTexture = this.texture;
    material.emissiveTexture.uOffset = -0.25; // more negative => bigger right shift.
    material.emissiveTexture.uScale  = 1.5;   // bigger scale => smaller text.
    material.emissiveTexture.vOffset = 1.2;   // > 1 because vScale must be negative.
    material.emissiveTexture.vScale  = -1.4;  // negative to compensate for right handed coordinate system in model.
    material.emissiveColor = new this.babylon.Color3(100,100,100); // With PBR, should multiply with emissive texture. Why such big values needed?
    material.diffuseColor = new this.babylon.Color3(1,1,1);
    material.environmentIntensity = 0.02;     // Bit of reflected environment in screen glass.

    // Load the model.
    // NOTE: At present there are two models with different geometry including different texture mapped
    //       and / or pickable objects:
    //  CC545: Texture on: DisplaySurface;  Pickables: RockerInnerCenter, RockerInnerLeft, RockerInnerRight; File: a-opt-cc545.glb
    // CO6602: Texture on: DisplaySurfaceL, DisplaySurfaceR; Pickables: None; File: a-opt-co6602.glb
    // The two models need different Y rotations and initial "look-at" positions.
    // However, apart from the model file name, no other changes are needed in this code. The name is currently hardcoded.

    let rockerPosition = 0;
    let baseUrl = document.location.pathname;
    let si = baseUrl.lastIndexOf("/");
    baseUrl = baseUrl.substring(0, si + 1);
    this.babylon.SceneLoader.ImportMesh("", baseUrl, `3d/${this.glbFileName}`, scene, newMeshes => {
      newMeshes[0].scaling = new me.babylon.Vector3(1.0, 1.0, 1.0);
      newMeshes[0].position.x = 0.0;
      newMeshes[0].position.y = 0.0;
      newMeshes[0].position.z = 0.0;
                                          
      newMeshes[0].rotation.x = 0.0;
      switch (this.consoleType) {
      case CyberConsole3D.CC545:
        newMeshes[0].rotation.y = 3.1415926; // right handed / left handed c.s. issue again.
        break;
      case CyberConsole3D.CO6602:
        newMeshes[0].rotation.y = 3.1415926/2.0;
        break;
      }
      newMeshes[0].rotation.z = 0.0;
                                          
      for (const mesh of newMeshes) {
        // Assign the material with the dynamic texture to the DisplaySurface.
        if (mesh.name === "DisplaySurface"
            || mesh.name === "DisplaySurfaceL"
            || mesh.name === "DisplaySurfaceR") {
          mesh.material = material;
        }

        // Hide Left and Right positioned rocker switch models. Center is default.
        if ((mesh.name === "RockerInnerLeft") || (mesh.name === "RockerInnerRight")) {
          mesh.setEnabled(false);
        }

        // Change active rocker switch model on double click pick of any rocker switch model.
        if ((mesh.name === "RockerInnerCenter") || (mesh.name === "RockerInnerLeft") || (mesh.name === "RockerInnerRight")) {
          mesh.actionManager = new me.babylon.ActionManager(scene);
          mesh.actionManager.registerAction(
            new me.babylon.ExecuteCodeAction(
              me.babylon.ActionManager.OnDoublePickTrigger, evt => {
                let meshNow = evt.meshUnderPointer;
                rockerPosition += 1;
                if (rockerPosition > 2) {
                  rockerPosition = 0;
                }

                // Show and hide appropriate rocker switch models.
                // Adjust texture coordinates so only selected display is visible.
                let rockerCenter = scene.getMeshByName("RockerInnerCenter");
                let rockerLeft = scene.getMeshByName("RockerInnerLeft");
                let rockerRight = scene.getMeshByName("RockerInnerRight");
                if (rockerPosition == 0) { // Center
                  rockerCenter.setEnabled(true);
                  rockerLeft.setEnabled(false);
                  rockerRight.setEnabled(false);
                  material.emissiveTexture.uOffset = -0.25 // Shift for A and B both visible.
                  material.emissiveTexture.uScale  = 1.5;  // Scale for A and B both visible.
                }
                if (rockerPosition == 1) { // Left
                  rockerCenter.setEnabled(false);
                  rockerLeft.setEnabled(true);
                  rockerRight.setEnabled(false);
                  material.emissiveTexture.uOffset = -0.16 // Shift for only A visible.
                  material.emissiveTexture.uScale  = 0.75; // Scale so A fills width.
                }
                if (rockerPosition == 2) { // Right
                  rockerCenter.setEnabled(false);
                  rockerLeft.setEnabled(false);
                  rockerRight.setEnabled(true);
                  material.emissiveTexture.uOffset = 0.35  // Shift for only B visible.
                  material.emissiveTexture.uScale  = 0.75; // Scale so B fills width.
                }
              }) // lambda ends.
          ); // registerAction ends.
        } // For a rocker mesh.
      } // over meshes
    }); // ImportMesh() ends.

    // Position the camera.
    switch (this.consoleType) {
    case CyberConsole3D.CC545:
      camera.setTarget(new this.babylon.Vector3(0.0, 0.11, 0.0));   // Where to look at initially. CC545
      break;
    case CyberConsole3D.CO6602:
      camera.setTarget(new this.babylon.Vector3(0.025, 0.34, 0.0)); // Where to look at initially. CO6602
      break;
    }
    camera.radius = 1.0;                // Initial camera distance = 1 meter.
    camera.wheelDeltaPercentage = 0.05; // Maybe improve mouse wheel speed.
    camera.upperRadiusLimit = 5.0;      // Maximum distance from target.
    camera.lowerRadiusLimit = 0.5;      // Minimum distace from target.

    return scene;
  }

  getContext() {
    return this.onscreenCanvas.getContext("webgl");
  }

  getEngine() {
    return this.engine;
  }

  updateScreen() {
    this.texture.update(); // Update the texture, thereby displaying the last drawn frame.
    this.clearScreen();    // Clear the last drawn frame ready for new input.
  }
}
