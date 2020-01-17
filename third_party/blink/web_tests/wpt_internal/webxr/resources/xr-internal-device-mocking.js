'use strict';

/* This file contains extensions to the base mocking from the WebPlatform tests
 * for interal tests. The main mocked objects are found in
 * ../external/wpt/resources/chromium/webxr-test.js. */
MockRuntime.prototype.setHitTestResults = function(results) {
  this.hittest_results_ = results;
};


// XREnvironmentIntegrationProvider implementation

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

MockRuntime.prototype.getSubmitFrameCount = function() {
  return this.presentation_provider_.submit_frame_count_;
};

MockRuntime.prototype.getMissingFrameCount = function() {
  return this.presentation_provider_.missing_frame_count_;
};

MockRuntime.prototype._injectAdditionalFrameData = function(options, frameData) {
  if (!options || !options.includeLightingEstimationData) {
    return;
  }

  frameData.lightEstimationData = {
    lightProbe: {
      sphericalHarmonics: {
        coefficients: new Array(9).fill().map((x, i) => { return { red: i, green: i, blue: i }; }),
      },
      mainLightDirection: { x: 0, y: 1, z: 0 },
      mainLightIntensity: { red: 1, green: 1, blue: 1 },
    },
    reflectionProbe: {
      cubeMap: {
        widthAndHeight: 16,
        positiveX: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        negativeX: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        positiveY: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        negativeY: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        positiveZ: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        negativeZ: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
      },
    },
  };
};

// Patch in experimental features.
MockRuntime.featureToMojoMap["dom-overlay"] =
    device.mojom.XRSessionFeature.DOM_OVERLAY;

ChromeXRTest.prototype.getService = function() {
  return this.mockVRService_;
};

MockVRService.prototype.setFramesThrottled = function(throttled) {
  return this.frames_throttled_ = throttled;
};

MockVRService.prototype.getFramesThrottled = function() {
  // Explicitly converted falsey states (i.e. undefined) to false.
  if (!this.frames_throttled_) {
    return false;
  }

  return this.frames_throttled_;
};

MockXRInputSource.prototype.getInputSourceStateCommon =
    MockXRInputSource.prototype.getInputSourceState;

MockXRInputSource.prototype.getInputSourceState = function() {
  let input_state = this.getInputSourceStateCommon();

  console.log('getInputSourceState this.overlay_pointer_position_=' + JSON.stringify(this.overlay_pointer_position_));
  if (this.overlay_pointer_position_) {
    input_state.overlayPointerPosition = this.overlay_pointer_position_;
  }

  console.log('input_state=' + JSON.stringify(input_state));
  return input_state;
};

MockXRInputSource.prototype.setOverlayPointerPosition = function(x, y) {
  this.overlay_pointer_position_ = { x: x, y: y };
};
