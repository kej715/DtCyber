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

class CyberConsole {

  constructor() {

    //
    // Font sizes
    //
    this.DOT_FONT        = 0;
    this.SMALL_FONT      = 1;
    this.MEDIUM_FONT     = 2;
    this.LARGE_FONT      = 3;

    //
    // Console screen offsets
    //
    this.SCREEN_GAP      = 40;
    this.SCREEN_MARGIN   = 20;

    //
    // Console stream commands
    //
    this.CMD_SET_X_LOW     = 0x80;
    this.CMD_SET_Y_LOW     = 0x81;
    this.CMD_SET_X_HIGH    = 0x82;
    this.CMD_SET_Y_HIGH    = 0x83;
    this.CMD_SET_SCREEN    = 0x84;
    this.CMD_SET_FONT_TYPE = 0x85;

    //
    // Console states
    //
    this.ST_TEXT           = 0;
    this.ST_COLLECT_SCREEN = 1;
    this.ST_COLLECT_FONT   = 2;
    this.ST_COLLECT_X      = 3;
    this.ST_COLLECT_Y      = 4;

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

  start() {
    const me = this;
    let iteration = 0;
    this.timer = setInterval(() => {
      const bitmap = me.offscreenCanvas.transferToImageBitmap();
      me.onscreenContext.transferFromImageBitmap(bitmap);
      me.clearScreen();
    }, 1000/10);
  }

  stop() {
    clearInterval(this.timer);
  }

  displayNotification(font, x, y, s) {
    this.stop();
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
    const bitmap = this.offscreenCanvas.transferToImageBitmap();
    this.onscreenContext.transferFromImageBitmap(bitmap);
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

  renderText(data) {

    if (typeof data === "string") {
      let ab = new Uint8Array(data.length);
      for (let i = 0; i < data.length; i++) {
        ab[i] = data.charCodeAt(i) & 0xff;
      }
      data = ab;
    }

    for (let i = 0; i < data.byteLength; i++) {
      let b = data[i];
      switch (this.state) {
      case this.ST_TEXT:
        if (b < 0x80) {
          if (this.currentFont === this.DOT_FONT) {
            this.drawPoint();
          }
          else {
            if (b < 0x20) b = 0x20;
            this.drawChar(b);
            this.x += this.fontWidths[this.currentFont];
          }
        }
        else {
          switch (b) {
          case this.CMD_SET_X_LOW:
            this.x = 0;
            this.state = this.ST_COLLECT_X;
            break;
          case this.CMD_SET_Y_LOW:
            this.y = 0;
            this.state = this.ST_COLLECT_Y;
            break;
          case this.CMD_SET_X_HIGH:
            this.x = 256;
            this.state = this.ST_COLLECT_X;
            break;
          case this.CMD_SET_Y_HIGH:
            this.y = 256;
            this.state = this.ST_COLLECT_Y;
            break;
          case this.CMD_SET_SCREEN:
            this.state = this.ST_COLLECT_SCREEN;
            break;
          case this.CMD_SET_FONT_TYPE:
            this.state = this.ST_COLLECT_FONT;
            break;
          default:
            // ignore the byte
            break;
          }
        }
        break;

      case this.ST_COLLECT_X:
        this.x = Math.round((this.x + b) * this.xRatio);
        this.state = this.ST_TEXT;
        break;

      case this.ST_COLLECT_Y:
        this.y = Math.round((0o777 - (this.y + b)) * this.yRatio);
        this.state = this.ST_TEXT;
        break;

      case this.ST_COLLECT_SCREEN:
        this.xOffset = (b === 1) ? this.screenWidth + this.SCREEN_GAP + this.SCREEN_MARGIN : this.SCREEN_MARGIN;
        this.state = this.ST_TEXT;
        break;

      case this.ST_COLLECT_FONT:
        this.setFont(b);
        this.state = this.ST_TEXT;
        break;

      default:
        // ignore byte
        break;
      }
    }
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
 *  Every 100ms, the "off screen" context is transferred to a texture mapped on a
 *  3D plane, which is rendered by Babylon.js.
 */

class CyberConsole3D extends CyberConsole {

  constructor(babylon) {
    super();
    this.babylon     = babylon;
    this.engine      = null;
    this.environMap  = "room-1.env";
    this.glbFileName = "a-opt-cc545.glb";
    this.scene       = null;
  }

  /*
   *  createScene - Create the scene (what we want Babylon.js to render).
   */
  createScene () {
    const me = this;
    let scene = new this.babylon.Scene(this.engine);

    // This is required for compatibility with objects created in Blender.
    // Also seems to turn on Euler rotations instead of using Quaternions.
    scene.useRightHandedSystem = true;

    // Apply post-rendering image processing tweaks. Do not use for now.
    //scene.imageProcessingConfiguration.contrast = 1.1;
    //scene.imageProcessingConfiguration.exposure = 1.0;
    //scene.imageProcessingConfiguration.toneMappingEnabled = false;

    // Add a camera. Let the pointer control the camera.
    let camera = new this.babylon.ArcRotateCamera("camera", -Math.PI / 2, Math.PI / 2.5, 10, new this.babylon.Vector3(0, 0, 0), scene);
    camera.minZ = 0.04; // Clip planes for better Z behaviour.
    camera.maxZ = 10.0;
    camera.fov = 0.4;   // Radians = about 23 degrees ~ 80mm on 35mm camera. Default 0.8 is too wide for this.
    camera.attachControl(this.onscreenCanvas, true);

    // Add lighting.
    let light = new this.babylon.HemisphericLight("light", new this.babylon.Vector3(1.5, 2, 3), scene);
    scene.environmentTexture = new this.babylon.CubeTexture(`3d/${this.environMap}`, scene);  // Needs more thought!

    // Create a dynamic texture incorporating the HTML canvas.
    let texture = new this.babylon.DynamicTexture("dynamicTexture", this.offscreenCanvas, scene, false);

    // Create a material with an emissive texture set to the dynamic texture.
    // Adjust texture coordinates to center the display.
    let material = new this.babylon.PBRMaterial("material", scene);
    material.emissiveTexture = texture;
    material.emissiveTexture.uOffset = -0.25 // more negative => bigger right shift.
    material.emissiveTexture.uScale  = 1.5;  // bigger scale => smaller text.
    material.emissiveTexture.vOffset = 1.2   // > 1 because vScale must be negative.
    material.emissiveTexture.vScale  = -1.4; // negative to compensate for right handed coordinate system in model.
    material.emissiveColor = new this.babylon.Color3(100,100,100); // With PBR, should multiply with emissive texture. Why such big value s needed?
    material.diffuseColor = new this.babylon.Color3(1,1,1);
    material.environmentIntensity = 0.02;    // Bit of reflected environment in screen glass.

    // Load the CC545 model.
    let baseUrl = document.location.pathname;
    let si = baseUrl.lastIndexOf("/");
    baseUrl = baseUrl.substring(0, si + 1);
    this.babylon.SceneLoader.ImportMesh("", baseUrl, `3d/${this.glbFileName}`, scene, newMeshes => {
      newMeshes[0].scaling = new me.babylon.Vector3(1.0, 1.0, 1.0);
      newMeshes[0].position.x = 0.0;
      newMeshes[0].position.y = 0.0;
      newMeshes[0].position.z = 0.0;
                                          
      newMeshes[0].rotation.x = 0.0;
      newMeshes[0].rotation.y = 3.1415926; // right handed / left handed c.s. issue again.
      newMeshes[0].rotation.z = 0.0;
                                          
      for (const mesh of newMeshes) {
        // The DisplaySurface is cc545_primitive7 in a-opt-cc545.glb ...
        // Assign the material with the dynamic texture to that.
        if (mesh.name === "cc545_primitive7") {
          mesh.material = material;
        }
      } // over meshes
    }); // ImportMesh() ends.

    // Update the texture from the off screen drawing at a regular interval.
    setInterval(() => {
      texture.update();  // Update the texture, thereby displaying the last drawn frame.
      me.clearScreen();  // Clear the last drawn frame ready for new input.
    }, 100);

    // Position the camera.
    camera.setTarget(new this.babylon.Vector3(0.0, 0.11, 0.0)); // Where to look at initially.
    camera.radius = 1.0;                                        // Initial camera distance = 1 meter.
    camera.wheelDeltaPercentage = 0.05;                         // Maybe imptove mouse wheel speed.
    camera.upperRadiusLimit = 5.0;                              // Maximum distance from target.
    camera.lowerRadiusLimit = 0.5; //-5.0;                      // Minimum distace from target.

    return scene;
  }

  displayNotification(font, x, y, s) {
    const lines = s.split("\n");
    this.setFont(font);
    this.notificationInterval = setInterval(() => {
      this.x           = x;
      this.xOffset     = 0;
      this.y           = y;
      this.state       = this.ST_TEXT;
      this.clearScreen();
      for (const line of s.split("\n")) {
        this.renderText(line);
        this.x  = x;
        this.y += this.fontHeights[this.currentFont];
      }
    }, 50);
  }

  stopNotification() {
    if (this.notificationInterval) {
      clearInterval(this.notificationInterval);
      this.notificationInterval = null;
    }
  }

  getContext() {
    return this.onscreenCanvas.getContext("webgl");
  }

  getEngine() {
    return this.engine;
  }

  start() {

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

  stop() {
    // No-op in 3d context
  }
}
