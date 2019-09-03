// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for mojo.
 */
cca.mojo = cca.mojo || {};

/**
 * Type definition for cca.mojo.PhotoCapabilities.
 * @extends {PhotoCapabilities}
 * @record
 */
cca.mojo.PhotoCapabilities = function() {};

/** @type {Array<string>} */
cca.mojo.PhotoCapabilities.prototype.supportedEffects;

/**
 * Creates the wrapper of JS image-capture and Mojo image-capture.
 * @param {!MediaStreamTrack} videoTrack A video track whose still images will
 *     be taken.
 * @param {!cca.mojo.MojoConnector} mojoConnector A mojo connector that could
 *     used to communicate with video capture device.
 * @constructor
 */
cca.mojo.ImageCapture = function(videoTrack, mojoConnector) {
  /**
   * The id of target media device.
   * @type {string}
   */
  this.deviceId_ = videoTrack.getSettings().deviceId;

  /**
   * The standard ImageCapture object.
   * @type {ImageCapture}
   * @private
   */
  this.capture_ = new ImageCapture(videoTrack);

  /**
   * The mojo connector that we used to negotiate with the video capture device.
   * @type {cca.mojo.MojoConnector}
   * @private
   */
  this.mojoConnector_ = mojoConnector;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Gets the photo capabilities with the available options/effects.
 * @return {!Promise<!cca.mojo.PhotoCapabilities>} Promise for the result.
 */
cca.mojo.ImageCapture.prototype.getPhotoCapabilities = async function() {
  const deviceOperator = await this.mojoConnector_.getDeviceOperator();
  const supportedEffects = [cros.mojom.Effect.NO_EFFECT];
  const isPortraitModeSupported =
      await deviceOperator.isPortraitModeSupported(this.deviceId_);
  if (isPortraitModeSupported) {
    supportedEffects.push(cros.mojom.Effect.PORTRAIT_MODE);
  }

  const baseCapabilities = await this.capture_.getPhotoCapabilities();

  let /** !cca.mojo.PhotoCapabilities */ extendedCapabilities;
  Object.assign(extendedCapabilities, baseCapabilities, {supportedEffects});
  return extendedCapabilities;
};

/**
 * Takes single or multiple photo(s) with the specified settings and effects.
 * The amount of result photo(s) depends on the specified settings and effects,
 * and the first promise in the returned array will always resolve with the
 * unreprocessed photo.
 * @param {!PhotoSettings} photoSettings Photo settings for ImageCapture's
 *     takePhoto().
 * @param {!Array<cros.mojom.Effect>=} photoEffects Photo effects to be applied.
 * @return {!Array<!Promise<!Blob>>} Array of promises for the result.
 */
cca.mojo.ImageCapture.prototype.takePhoto = function(
    photoSettings, photoEffects = []) {
  const takes = [];
  if (photoEffects) {
    for (const effect of photoEffects) {
      const take = (async () => {
        const deviceOperator = await this.mojoConnector_.getDeviceOperator();
        const device = deviceOperator.getDevice(this.deviceId_);
        const {status, blob} = await device.setReprocessOption(effect);
        if (status !== 0) {
          throw new Error('Mojo image capture error: ' + status);
        }
        const {data, mimeType} = blob;
        return new Blob([new Uint8Array(data)], {type: mimeType});
      })();
      takes.push(take);
    }
  }
  takes.splice(0, 0, this.capture_.takePhoto(photoSettings));
  return takes;
};
