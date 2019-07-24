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
 * The mojo interface of CrOS Image Capture API. It provides a singleton
 * instance.
 */
cca.mojo.MojoInterface = class {
  /** @public */
  constructor() {
    /**
     * @type {cros.mojom.CrosImageCaptureProxy} A interface proxy that used to
     *     construct the mojo interface.
     */
    this.proxy = cros.mojom.CrosImageCapture.getProxy();
  }

  /**
   * Gets the mojo interface proxy which could be used to communicate with
   * Chrome.
   * @return {cros.mojom.CrosImageCaptureProxy} The mojo interface proxy.
   */
  static getProxy() {
    if (!cca.mojo.MojoInterface.instance_) {
      /**
       * @type {cca.mojo.MojoInterface} The singleton instance of this object.
       */
      cca.mojo.MojoInterface.instance_ = new cca.mojo.MojoInterface();
    }
    return cca.mojo.MojoInterface.instance_.proxy;
  }
};

/**
 * Creates the wrapper of JS image-capture and Mojo image-capture.
 * @param {!MediaStreamTrack} videoTrack A video track whose still images will
 *     be taken.
 * @constructor
 */
cca.mojo.ImageCapture = function(videoTrack) {
  /**
   * @type {string} The id of target media device.
   */
  this.deviceId_ = cca.mojo.ImageCapture.resolveDeviceId(videoTrack) || '';

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
 *     query the data.
 * @param {cros.mojom.CameraMetadataTag} tag Camera metadata tag to query for.
 * @return {Array<number>} An array containing elements whose types correspond
 *     to the format of input |tag|. If nothing is found, returns an empty
 *     array.
 * @private
 */
cca.mojo.getMetadataData_ = function(metadata, tag) {
  for (let i = 0; i < metadata.entryCount; i++) {
    let entry = metadata.entries[i];
    if (entry.tag !== tag) {
      continue;
    }
    const {buffer} = Uint8Array.from(entry.data);
    switch (entry.type) {
      case cros.mojom.EntryType.TYPE_BYTE:
        return Array.from(new Uint8Array(buffer));
      case cros.mojom.EntryType.TYPE_INT32:
        return Array.from(new Int32Array(buffer));
      case cros.mojom.EntryType.TYPE_FLOAT:
        return Array.from(new Float32Array(buffer));
      case cros.mojom.EntryType.TYPE_DOUBLE:
        return Array.from(new Float64Array(buffer));
      case cros.mojom.EntryType.TYPE_INT64:
        return Array.from(new BigInt64Array(buffer), (bigIntVal) => {
          let numVal = Number(bigIntVal);
          if (!Number.isSafeInteger(numVal)) {
            console.warn('The int64 value is not a safe integer');
          }
          return numVal;
        });
      default:
        // TODO(wtlee): Supports rational type.
        throw new Error('Unsupported type: ' + entry.type);
    }
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
cca.mojo.ImageCapture.prototype.getPhotoCapabilities = async function() {
  // TODO(wtlee): Change to portrait mode tag.
  // This should be 0x80000000 but mojo interface will convert the tag to int32.
  const portraitModeTag =
      /** @type{cros.mojom.CameraMetadataTag} */ (-0x80000000);

  let [/** @type {cca.mojo.PhotoCapabilities} */capabilities,
       {/** @type {cros.mojom.CameraInfo} */cameraInfo},
  ] = await Promise.all([
    this.capture_.getPhotoCapabilities(),
    cca.mojo.MojoInterface.getProxy().getCameraInfo(this.deviceId_),
  ]);

  if (cameraInfo === null) {
    throw new Error('No photo capabilities is found for given device id.');
  }
  const staticMetadata = cameraInfo.staticCameraCharacteristics;
  let supportedEffects = [cros.mojom.Effect.NO_EFFECT];
  if (cca.mojo.getMetadataData_(staticMetadata, portraitModeTag).length > 0) {
    supportedEffects.push(cros.mojom.Effect.PORTRAIT_MODE);
  }
  capabilities.supportedEffects = supportedEffects;
  return capabilities;
};

/**
 * Takes single or multiple photo(s) with the specified settings and effects.
 * The amount of result photo(s) depends on the specified settings and effects,
 * and the first promise in the returned array will always resolve with the
 * unreprocessed photo.
 * @param {!PhotoSettings} photoSettings Photo settings for ImageCapture's
 *     takePhoto().
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
 *     which could be retrieved from MediaDeviceInfo.deviceId.
 * @return {Promise<Array<Object>>} Promise of supported resolutions. Each
 *     photo resolution is represented as [width, height].
 */
cca.mojo.getPhotoResolutions = async function(deviceId) {
  const formatBlob = 33;
  const typeOutputStream = 0;
  const numElementPerEntry = 4;

  let {cameraInfo} =
      await cca.mojo.MojoInterface.getProxy().getCameraInfo(deviceId);
  if (cameraInfo === null) {
    throw new Error('No photo resolutions is found for given device id.');
  }

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
  for (let i = 0; i < streamConfigs.length; i += numElementPerEntry) {
    const [format, width, height, type] =
        streamConfigs.slice(i, i + numElementPerEntry);
    if (format === formatBlob && type === typeOutputStream) {
      supportedResolutions.push([width, height]);
    }
  }
  return supportedResolutions;
};

/**
 * Gets supported video configurations for specific camera.
 * @param {string} deviceId The renderer-facing device Id of the target camera
 *     which could be retrieved from MediaDeviceInfo.deviceId.
 * @return {Promise<Array<Object>>} Promise of supported video configurations.
 *     Each configuration is represented as [width, height, maxFps].
 */
cca.mojo.getVideoConfigs = async function(deviceId) {
  // Currently we use YUV format for both recording and previewing on Chrome.
  const formatYuv = 35;
  const oneSecondInNs = 1000000000;
  const numElementPerEntry = 4;

  let {cameraInfo} =
      await cca.mojo.MojoInterface.getProxy().getCameraInfo(deviceId);
  if (cameraInfo === null) {
    throw new Error('No video configs is found for given device id.');
  }
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
  for (let i = 0; i < minFrameDurationConfigs.length; i += numElementPerEntry) {
    const [format, width, height, minDuration] =
        minFrameDurationConfigs.slice(i, i + numElementPerEntry);
    if (format === formatYuv) {
      const maxFps = Math.round(oneSecondInNs / minDuration);
      supportedConfigs.push([width, height, maxFps]);
    }
  }
  return supportedConfigs;
};

/**
 * Gets camera facing for given device.
 * @param {string} deviceId The renderer-facing device Id of the target camera
 *     which could be retrieved from MediaDeviceInfo.deviceId.
 * @return {Promise<cros.mojom.CameraFacing>} Promise of device facing.
 */
cca.mojo.getCameraFacing = async function(deviceId) {
  let {cameraInfo} =
      await cca.mojo.MojoInterface.getProxy().getCameraInfo(deviceId);
  if (cameraInfo === null) {
    throw new Error('No camera facing is found for given device id.');
  }
  return cameraInfo.facing;
};

/**
 * Gets supported fps ranges for specific camera.
 * @param {string} deviceId The renderer-facing device Id of the target camera
 *     which could be retrieved from MediaDeviceInfo.deviceId.
 * @return {Promise<Array<Array<number>>>} Promise of supported fps ranges.
 *     Each range is represented as [min, max].
 */
cca.mojo.getSupportedFpsRanges = async function(deviceId) {
  const numElementPerEntry = 2;

  let {cameraInfo} =
      await cca.mojo.MojoInterface.getProxy().getCameraInfo(deviceId);
  if (cameraInfo === null) {
    throw new Error('No supported Fps Ranges is found for given device id.');
  }

  const staticMetadata = cameraInfo.staticCameraCharacteristics;
  const availableFpsRanges = cca.mojo.getMetadataData_(
      staticMetadata,
      cros.mojom.CameraMetadataTag
          .ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
  // The data of |availableFpsRanges| looks like:
  // availableFpsRanges: [RANGE_1_MIN, RANGE_1_MAX,
  //                      RANGE_2_MIN, RANGE_2_MAX, ...]
  if (availableFpsRanges.length % numElementPerEntry != 0) {
    throw new Error('Unexpected length of available fps range configs');
  }

  let supportedFpsRanges = [];
  for (let i = 0; i < availableFpsRanges.length; i += numElementPerEntry) {
    const [rangeMin, rangeMax] =
        availableFpsRanges.slice(i, i + numElementPerEntry);
    supportedFpsRanges.push([rangeMin, rangeMax]);
  }
  return supportedFpsRanges;
};

/**
 * Gets user media with custom negotiation through CrosImageCapture API,
 * such as frame rate range negotiation.
 * @param {string} deviceId The renderer-facing device Id of the target camera
 *     which could be retrieved from MediaDeviceInfo.deviceId.
 * @param {!MediaStreamConstraints} constraints The constraints that would be
 *     passed to get user media. If frame rate range negotiation is needed, the
 *     caller should either set exact field or set both min and max fields for
 *     frame rate property.
 * @return {Promise<MediaStream>} Promise of the MediaStream that returned from
 *     MediaDevices.getUserMedia().
 */
cca.mojo.getUserMedia = async function(deviceId, constraints) {
  let streamWidth = 0;
  let streamHeight = 0;
  let minFrameRate = 0;
  let maxFrameRate = 0;

  try {
    if (constraints && constraints.video && constraints.video.frameRate) {
      const frameRate = constraints.video.frameRate;
      if (frameRate.exact) {
        minFrameRate = frameRate.exact;
        maxFrameRate = frameRate.exact;
      } else if (frameRate.min && frameRate.max) {
        minFrameRate = frameRate.min;
        maxFrameRate = frameRate.max;
      }

      streamWidth = constraints.video.width;
      if (typeof streamWidth !== 'number') {
        throw new Error('streamWidth expected to be a number');
      }
      streamHeight = constraints.video.height;
      if (typeof streamHeight !== 'number') {
        throw new Error('streamHeight expected to be a number');
      }
    }

    let hasSpecifiedFrameRateRange = minFrameRate > 0 && maxFrameRate > 0;
    // If the frame rate range is specified in |constraints|, we should try to
    // set the frame rate range and should report error if fails since it is
    // unexpected.
    //
    // Otherwise, if the frame rate is incomplete or totally missing in
    // |constraints| , we assume the app wants to use default frame rate range.
    // We set the frame rate range to an invalid range (e.g. 0 fps) so that it
    // will fallback to use the default one.
    const {isSuccess} = await cca.mojo.MojoInterface.getProxy().setFpsRange(
        deviceId, streamWidth, streamHeight, minFrameRate, maxFrameRate);

    if (!isSuccess && hasSpecifiedFrameRateRange) {
      console.error('Failed to negotiate the frame rate range.');
    }
  } catch (e) {
    // Ignore HALv1 Error.
  }
  return navigator.mediaDevices.getUserMedia(constraints);
};
