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
 * @typedef {PhotoCapabilities} cca.mojo.PhotoCapabilities
 * @property {Array<string>} [supportedEffects]
 */
cca.mojo.PhotoCapabilities;

/**
 * The mojo interface of CrOS Image Capture API. It provides a singleton
 * instance.
 */
cca.mojo.MojoInterface = function() {
  /**
   * @type {cca.mojo.MojoInterface} The singleton instance of this object.
   */
  cca.mojo.MojoInterface.instance_ = this;

  /**
   * @type {cros.mojom.CrosImageCaptureProxy} A interface proxy that used to
   *     construct the mojo interface.
   */
  this.proxy = cros.mojom.CrosImageCapture.getProxy();

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Gets the mojo interface proxy which could be used to communicate with Chrome.
 * @return {cros.mojom.CrosImageCaptureProxy} The mojo interface proxy.
 */
cca.mojo.MojoInterface.getProxy = function() {
  if (!cca.mojo.MojoInterface.instance_) {
    new cca.mojo.MojoInterface();
  }
  return cca.mojo.MojoInterface.instance_.proxy;
};

/**
 * Creates the wrapper of JS image-capture and Mojo image-capture.
 * @param {MediaStreamTrack} videoTrack A video track whose still images will be
 *     taken.
 * @constructor
 */
cca.mojo.ImageCapture = function(videoTrack) {
  /**
   * @type {string} The id of target media device.
   */
  this.deviceId_ = cca.mojo.ImageCapture.resolveDeviceId(videoTrack);

  /**
   * @type {ImageCapture}
   * @private
   */
  this.capture_ = new ImageCapture(videoTrack);

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Gets the data from Camera metadata by its tag.
 * @param {cros.mojom.CameraMetadata} metadata Camera metadata from which to
 *   query the data.
 * @param {cros.mojom.CameraMetadataTag} tag Camera metadata tag to query for.
 * @return {Array<Object>} An array containing elements whose types correspond
 *   to the format of input |tag|. If nothing is found, returns an empty array.
 * @private
 */
cca.mojo.getMetadataData_ = function(metadata, tag) {
  const size4Bytes = 4;
  const size8Bytes = 8;

  for (let i = 0; i < metadata.entryCount; i++) {
    let entry = metadata.entries[i];
    if (entry.tag !== tag) {
      continue;
    }
    let uint8Array = Uint8Array.from(entry.data);
    let dataView = new DataView(uint8Array.buffer);
    let results = [];
    let mostSignificantInt32;
    let leastSignificantInt32;
    for (let j = 0; j < entry.count; j++) {
      switch (entry.type) {
        case cros.mojom.EntryType.TYPE_BYTE:
          results.push(dataView.getUint8(j, true));
          break;
        case cros.mojom.EntryType.TYPE_INT32:
          results.push(dataView.getInt32(j * size4Bytes, true));
          break;
        case cros.mojom.EntryType.TYPE_FLOAT:
          results.push(dataView.getFloat32(j * size4Bytes, true));
          break;
        case cros.mojom.EntryType.TYPE_DOUBLE:
          results.push(dataView.getFloat64(j * size8Bytes, true));
          break;
        case cros.mojom.EntryType.TYPE_INT64:
          // TODO(wtlee): Currently int64 value will fallback to int32 by
          // picking only the least significant 32 bits. Need to find a way
          // to better handle int64 values.
          leastSignificantInt32 = dataView.getInt32(j * size8Bytes, true);
          mostSignificantInt32 = dataView.getInt32(j * size8Bytes + 4, true);
          if (mostSignificantInt32 !== 0) {
            console.warn('Truncate non-zero most significant bytes');
          }
          results.push(leastSignificantInt32);
          break;
        default:
          // TODO(wtlee): Supports rational type.
          throw new Error('Unsupported type: ' + entry.type);
      }
    }
    return results;
  }
  return [];
};

/**
 * Resolves video device id from its video track.
 * @param {MediaStreamTrack} videoTrack Video track of device to be resolved.
 * @return {?string} Resolved video device id. Returns null for unable to
 *     resolve.
 */
cca.mojo.ImageCapture.resolveDeviceId = function(videoTrack) {
  const trackSettings = videoTrack.getSettings && videoTrack.getSettings();
  return trackSettings && trackSettings.deviceId || null;
};

/**
 * Gets the photo capabilities with the available options/effects.
 * @return {Promise<cca.mojo.PhotoCapabilities>} Promise for the result.
 */
cca.mojo.ImageCapture.prototype.getPhotoCapabilities = function() {
  // TODO(wtlee): Change to portrait mode tag.
  // This should be 0x80000000 but mojo interface will convert the tag to int32.
  const portraitModeTag = -0x80000000;
  return Promise
      .all([
        this.capture_.getPhotoCapabilities(),
        cca.mojo.MojoInterface.getProxy().getCameraInfo(this.deviceId_),
      ])
      .then(([capabilities, {cameraInfo}]) => {
        const staticMetadata = cameraInfo.staticCameraCharacteristics;
        let supportedEffects = [cros.mojom.Effect.NO_EFFECT];
        if (cca.mojo.getMetadataData_(staticMetadata, portraitModeTag).length >
            0) {
          supportedEffects.push(cros.mojom.Effect.PORTRAIT_MODE);
        }
        capabilities.supportedEffects = supportedEffects;
        return capabilities;
      });
};

/**
 * Takes single or multiple photo(s) with the specified settings and effects.
 * The amount of result photo(s) depends on the specified settings and effects,
 * and the first promise in the returned array will always resolve with the
 * unreprocessed photo.
 * @param {?Object} photoSettings Photo settings for ImageCapture's takePhoto().
 * @param {?Array<cros.mojom.Effect>} photoEffects Photo effects to be applied.
 * @return {Array<Promise<Blob>>} Array of promises for the result.
 */
cca.mojo.ImageCapture.prototype.takePhoto = function(
    photoSettings, photoEffects) {
  const takes = [];
  if (photoEffects) {
    photoEffects.forEach((effect) => {
      takes.push((cca.mojo.MojoInterface.getProxy().setReprocessOption(
                      this.deviceId_, effect))
                     .then(({status, blob}) => {
                       if (status != 0) {
                         throw new Error('Mojo image capture error: ' + status);
                       }
                       const {data, mimeType} = blob;
                       return new Blob(
                           [new Uint8Array(data)], {type: mimeType});
                     }));
    });
  }
  takes.splice(0, 0, this.capture_.takePhoto(photoSettings));
  return takes;
};

/**
 * Gets supported photo resolutions for specific camera.
 * @param {string} deviceId The renderer-facing device Id of the target camera
 *   which could be retrieved from MediaDeviceInfo.deviceId.
 * @return {Promise<Array<Object>>} Promise of supported resolutions. Each
 *   photo resolution is represented as [width, height].
 */
cca.mojo.getPhotoResolutions = function(deviceId) {
  const formatBlob = 33;
  const typeOutputStream = 0;
  const formatIndex = 0;
  const widthIndex = 1;
  const heightIndex = 2;
  const typeIndex = 3;
  const numElementPerEntry = 4;

  return cca.mojo.MojoInterface.getProxy().getCameraInfo(deviceId).then(
      ({cameraInfo}) => {
        const staticMetadata = cameraInfo.staticCameraCharacteristics;
        const streamConfigs = cca.mojo.getMetadataData_(
            staticMetadata,
            cros.mojom.CameraMetadataTag
                .ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
        // The data of |streamConfigs| looks like:
        // streamConfigs: [FORMAT_1, WIDTH_1, HEIGHT_1, TYPE_1,
        //                 FORMAT_2, WIDTH_2, HEIGHT_2, TYPE_2, ...]
        if (streamConfigs.length % numElementPerEntry != 0) {
          throw new Error('Unexpected length of stream configurations');
        }

        let supportedResolutions = [];
        for (let configIdx = 0, configBase = 0;
             configBase < streamConfigs.length;
             configIdx++, configBase = configIdx * numElementPerEntry) {
          const format = streamConfigs[configBase + formatIndex];
          if (format === formatBlob) {
            const type = streamConfigs[configBase + typeIndex];
            if (type === typeOutputStream) {
              const width = streamConfigs[configBase + widthIndex];
              const height = streamConfigs[configBase + heightIndex];
              supportedResolutions.push([width, height]);
            }
          }
        }
        return supportedResolutions;
      });
};

/**
 * Gets supported video configurations for specific camera.
 * @param {string} deviceId The renderer-facing device Id of the target camera
 *   which could be retrieved from MediaDeviceInfo.deviceId.
 * @return {Promise<Array<Object>>} Promise of supported video configurations.
 * Each configuration is represented as [width, height, fps].
 */
cca.mojo.getVideoConfigs = function(deviceId) {
  // Currently we use YUV format for both recording and previewing on Chrome.
  const formatYuv = 35;
  const oneSecondInNs = 1000000000;
  const formatIndex = 0;
  const widthIndex = 1;
  const heightIndex = 2;
  const durationIndex = 3;
  const numElementPerEntry = 4;

  return cca.mojo.MojoInterface.getProxy().getCameraInfo(deviceId).then(
      ({cameraInfo}) => {
        const staticMetadata = cameraInfo.staticCameraCharacteristics;
        const minFrameDurationConfigs = cca.mojo.getMetadataData_(
            staticMetadata,
            cros.mojom.CameraMetadataTag
                .ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
        // The data of |minFrameDurationConfigs| looks like:
        // minFrameDurationCOnfigs: [FORMAT_1, WIDTH_1, HEIGHT_1, DURATION_1,
        //                           FORMAT_2, WIDTH_2, HEIGHT_2, DURATION_2,
        //                           ...]
        if (minFrameDurationConfigs.length % numElementPerEntry != 0) {
          throw new Error('Unexpected length of frame durations configs');
        }

        let supportedConfigs = [];
        for (let configIdx = 0, configBase = 0;
             configBase < minFrameDurationConfigs.length;
             configIdx++, configBase = configIdx * numElementPerEntry) {
          const format = minFrameDurationConfigs[configBase + formatIndex];
          if (format === formatYuv) {
            const width = minFrameDurationConfigs[configBase + widthIndex];
            const height = minFrameDurationConfigs[configBase + heightIndex];
            const fps = Math.round(
                oneSecondInNs /
                minFrameDurationConfigs[configBase + durationIndex]);
            supportedConfigs.push([width, height, fps]);
          }
        }
        return supportedConfigs;
      });
};

/**
 * Gets camera facing for given device.
 * @param {string} deviceId The renderer-facing device Id of the target camera
 *   which could be retrieved from MediaDeviceInfo.deviceId.
 * @return {Promise<cros.mojom.CameraFacing>} Promise of device facing.
 */
cca.mojo.getCameraFacing = function(deviceId) {
  return cca.mojo.MojoInterface.getProxy().getCameraInfo(deviceId).then(
      ({cameraInfo}) => {
        return cameraInfo.facing;
      });
};
