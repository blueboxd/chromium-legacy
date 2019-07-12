'use strict';

/* This file contains extensions to the base mocking from the WebPlatform tests
 * for interal tests. The main mocked objects are found in
 * ../external/wpt/resources/chromium/webxr-test.js. */
MockRuntime.prototype.setHitTestResults = function(results) {
  this.hittest_results_ = results;
};

MockRuntime.prototype.requestHitTest = function(ray) {
  var hit_results = this.hittest_results_;
  if (!hit_results) {
    var hit = new device.mojom.XRHitResult();

    // No change to the underlying matrix/leaving it null results in identity.
    hit.hitMatrix = new gfx.mojom.Transform();
    hit_results = {results: [hit]};
  }
  return Promise.resolve(hit_results);
};

MockRuntime.prototype.setStageSize = function(x, z) {
  if (!this.displayInfo_.stageParameters) {
    this.displayInfo_.stageParameters = default_stage_parameters;
  }

  this.displayInfo_.stageParameters.sizeX = x;
  this.displayInfo_.stageParameters.sizeZ = z;

  this.sessionClient_.onChanged(this.displayInfo_);
};

MockRuntime.prototype.getSubmitFrameCount = function() {
  return this.presentation_provider_.submit_frame_count_;
};

MockRuntime.prototype.getMissingFrameCount = function() {
  return this.presentation_provider_.missing_frame_count_;
};

MockXRInputSource.prototype.connectGamepad = function() {
  // Mojo complains if some of the properties on Gamepad are null, so set
  // everything to reasonable defaults that tests can override.
  this.gamepad_ = new device.mojom.Gamepad();
  this.gamepad_.connected = true;
  this.gamepad_.id = "unknown";
  this.gamepad_.timestamp = 0;
  this.gamepad_.axes = [];
  this.gamepad_.buttons = [];
  this.gamepad_.mapping = "";
  this.gamepad_.display_id = 0;
  this.gamepad_.hand = device.mojom.GamepadHand.GamepadHandNone;
};

MockXRInputSource.prototype.disconnectGamepad = function() {
  this.gamepad_ = null;
};

MockXRInputSource.prototype.setGamepadButtonCount = function(button_count) {
  this.gamepad_.buttons = [];
  for (let i = 0; i < button_count; ++i) {
    this.gamepad_.buttons.push(new device.mojom.GamepadButton());
  }
};

MockXRInputSource.prototype.setGamepadAxesCount = function(axes_count) {
  this.gamepad_.axes = [];
  for (let i = 0; i < axes_count; ++i) {
    this.gamepad_.axes.push(0);
  }
};

MockXRInputSource.prototype.setGamepadButtonPressed = function(button_index, is_pressed) {
  this.gamepad_.buttons[button_index].pressed = is_pressed;
};

MockXRInputSource.prototype.setGamepadAxisValue = function(index, value) {
  this.gamepad_.axes[index] = value;
};
