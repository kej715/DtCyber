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
   *  There are two drawing canvasses and associated contexts:
   *  1. The "on screen" canvas/context which is mapped to a visible HTML canvas.
   *     This uses a bitmaprenderer context and is not drawn into directly.
   *  2. The "off screen" canvas/context into which things are drawn using standard
   *     HTML 5 functions.
   *  Every 100ms, a bitmap is created from "off screen" context and transferred
   *  to the "on screen" context.
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
 *  implement 3d graphics.
 */

class CyberConsole3D extends CyberConsole {

  constructor(babylon) {
    super();
    this.babylon = babylon;
    this.engine = null;
    this.scene = null;
  }

  /*
   *  createScene - Create the scene (what we want Babylon.js to render).
   */
  createScene () {
    const me = this;
    let scene = new this.babylon.Scene(this.engine);

    // Apply post-rendering image processing tweaks.
    scene.imageProcessingConfiguration.contrast = 1.1;
    scene.imageProcessingConfiguration.exposure = 1.0;
    scene.imageProcessingConfiguration.toneMappingEnabled = false;

    // Add a camera and a light. Let the pointer cotrol the camera.
    let camera = new this.babylon.ArcRotateCamera("camera", -Math.PI / 2, Math.PI / 2.5, 10, new this.babylon.Vector3(0, 0, 0), scene);
    camera.minZ = 0.04;
    camera.maxZ = 100.0;
    camera.attachControl(this.onscreenCanvas, true);

    let light = new this.babylon.HemisphericLight("light", new this.babylon.Vector3(1, 1, 0), scene);

    // Create a plane for the terminal screen
    let plane = this.babylon.MeshBuilder.CreatePlane("plane", { size: 5 }, scene);

    // Create a dynamic texture incorporating the HTML canvas.
    let texture = new this.babylon.DynamicTexture("dynamicTexture", this.offscreenCanvas, scene, false);

    // Apply the dynamic texture to the plane
    let material = new this.babylon.StandardMaterial("material", scene);
    material.emissiveTexture = texture;
    material.diffuseColor = new this.babylon.Color3(0.2,0.2,0.2);
    //material.ambientTexture = texture;
    plane.material = material;

    // Update the texture from the off screen drawing at a regular interval.
    setInterval(() => {
      texture.update();  // Update the texture, thereby displaying the last drawn frame.
      me.clearScreen();  // Clear the last drawn frame ready for new input.
    }, 120);

    // Position the camera to face the plane
    camera.setTarget(plane.position);
    camera.radius = 5; // Adjust the camera distance

    return scene;
  }

  /*
   *  createScreen
   *
   *  There are two drawing canvasses and associated contexts:
   *  1. The "on screen" canvas/context which can be used by the Babylon.js render engine.
   *     This uses a WebGL context and is not drawn into directly.
   *  2. The "off screen" canvas/context into which things are drawn using standard
   *     HTML 5 functions.
   *  Every 100ms, the "off screen" context is transferred to a texture mapped on a
   *  3D plane, which is rendered by Babylon.js.
   */
  createScreen() {
    super.createScreen();
  }

  displayNotification(font, x, y, s) {
    this.setFont(font);
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
