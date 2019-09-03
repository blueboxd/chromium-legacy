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
 * Operates video capture device through CrOS Camera App Mojo interface.
 */
cca.mojo.DeviceOperator = class {
  /**
   * @public
   * @param {!Map<string, !cros.mojom.CameraAppDeviceRemote>} devices Map of
   *     device remotes.
   */
  constructor(devices) {
    /**
     * A map that stores the device remotes that could be used to communicate
     * with camera device through mojo interface.
     * @type {!Map<string, !cros.mojom.CameraAppDeviceRemote>}
     */
    this.devices_ = devices;
  }

  /**
   * Gets corresponding device remote by given id.
   * @param {string} deviceId The id for target camera device.
   * @return {!cros.mojom.CameraAppDeviceRemote} Corresponding device remote.
   * @throws {Error} Thrown when the given deviceId does not found in the map.
   */
  getDevice(deviceId) {
    const device = this.devices_.get(deviceId);
    if (device === undefined) {
      throw new Error('Invalid deviceId: ', deviceId);
    }
    return device;
  }

  /**
   * Gets supported photo resolutions for specific camera.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!Array<!Array<number>>>} Promise of supported
   *     resolutions. Each photo resolution is represented as [width, height].
   * @throws {Error} Thrown when fail to parse the metadata.
   */
  async getPhotoResolutions(deviceId) {
    const formatBlob = 33;
    const typeOutputStream = 0;
    const numElementPerEntry = 4;

    const device = this.getDevice(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
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

    const supportedResolutions = [];
    for (let i = 0; i < streamConfigs.length; i += numElementPerEntry) {
      const [format, width, height, type] =
          streamConfigs.slice(i, i + numElementPerEntry);
      if (format === formatBlob && type === typeOutputStream) {
        supportedResolutions.push([width, height]);
      }
    }
    return supportedResolutions;
  }

  /**
   * Gets supported video configurations for specific camera.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!Array<!Array<number>>>} Promise of supported video
   *     configurations. Each configuration is represented as [width, height,
   *     maxFps].
   * @throws {Error} Thrown when fail to parse the metadata.
   */
  async getVideoConfigs(deviceId) {
    // Currently we use YUV format for both recording and previewing on Chrome.
    const formatYuv = 35;
    const oneSecondInNs = 1e9;
    const numElementPerEntry = 4;

    const device = this.getDevice(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
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

    const supportedConfigs = [];
    for (let i = 0; i < minFrameDurationConfigs.length;
         i += numElementPerEntry) {
      const [format, width, height, minDuration] =
          minFrameDurationConfigs.slice(i, i + numElementPerEntry);
      if (format === formatYuv) {
        const maxFps = Math.round(oneSecondInNs / minDuration);
        supportedConfigs.push([width, height, maxFps]);
      }
    }
    return supportedConfigs;
  }

  /**
   * Gets camera facing for given device.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!cros.mojom.CameraFacing>} Promise of device facing.
   */
  async getCameraFacing(deviceId) {
    const device = this.getDevice(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
    return cameraInfo.facing;
  }

  /**
   * Gets supported fps ranges for specific camera.
   * @param {string} deviceId The renderer-facing device id of the target camera
   *     which could be retrieved from MediaDeviceInfo.deviceId.
   * @return {!Promise<!Array<Array<number>>>} Promise of supported fps ranges.
   *     Each range is represented as [min, max].
   * @throws {Error} Thrown when fail to parse the metadata.
   */
  async getSupportedFpsRanges(deviceId) {
    const numElementPerEntry = 2;

    const device = this.getDevice(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
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

    const supportedFpsRanges = [];
    for (let i = 0; i < availableFpsRanges.length; i += numElementPerEntry) {
      const [rangeMin, rangeMax] =
          availableFpsRanges.slice(i, i + numElementPerEntry);
      supportedFpsRanges.push([rangeMin, rangeMax]);
    }
    return supportedFpsRanges;
  }

  /**
   * Sets the fps range for target device.
   * @param {string} deviceId
   * @param {MediaStreamConstraints} constraints The constraints including fps
   *     range and the resolution. If frame rate range negotiation is needed,
   *     the caller should either set exact field or set both min and max fields
   *     for frame rate property.
   * @throws {Error} Thrown when the input contains invalid values.
   */
  async setFpsRange(deviceId, constraints) {
    let /** number */ streamWidth = 0;
    let /** number */ streamHeight = 0;
    let /** number */ minFrameRate = 0;
    let /** number */ maxFrameRate = 0;

    if (constraints && constraints.video && constraints.video.frameRate) {
      const frameRate = constraints.video.frameRate;
      if (frameRate.exact) {
        minFrameRate = frameRate.exact;
        maxFrameRate = frameRate.exact;
      } else if (frameRate.min && frameRate.max) {
        minFrameRate = frameRate.min;
        maxFrameRate = frameRate.max;
      }
      // TODO(wtlee): To set the fps range to the default value, we should
      // remove the frameRate from constraints instead of using incomplete
      // range.

      // We only support number type for width and height. If width or height
      // is other than a number (e.g. ConstrainLong, undefined, etc.), we should
      // throw an error.
      if (typeof constraints.video.width !== 'number') {
        throw new Error('width in constraints is expected to be a number');
      }
      streamWidth = constraints.video.width;

      if (typeof constraints.video.height !== 'number') {
        throw new Error('height in constraints is expected to be a number');
      }
      streamHeight = constraints.video.height;
    }

    const hasSpecifiedFrameRateRange = minFrameRate > 0 && maxFrameRate > 0;
    // If the frame rate range is specified in |constraints|, we should try to
    // set the frame rate range and should report error if fails since it is
    // unexpected.
    //
    // Otherwise, if the frame rate is incomplete or totally missing in
    // |constraints| , we assume the app wants to use default frame rate range.
    // We set the frame rate range to an invalid range (e.g. 0 fps) so that it
    // will fallback to use the default one.
    const device = this.getDevice(deviceId);
    const {isSuccess} = await device.setFpsRange(
        {'width': streamWidth, 'height': streamHeight},
        {'start': minFrameRate, 'end': maxFrameRate});

    if (!isSuccess && hasSpecifiedFrameRateRange) {
      console.error('Failed to negotiate the frame rate range.');
    }
  }

  /**
   * Checks if portrait mode is supported.
   * @param {string} deviceId The id for target device.
   * @return {!Promise<boolean>} Promise of the boolean result.
   */
  async isPortraitModeSupported(deviceId) {
    // TODO(wtlee): Change to portrait mode tag.
    // This should be 0x80000000 but mojo interface will convert the tag to
    // int32.
    const portraitModeTag =
        /** @type{cros.mojom.CameraMetadataTag} */ (-0x80000000);

    const device = this.getDevice(deviceId);
    const {cameraInfo} = await device.getCameraInfo();
    return cca.mojo
               .getMetadataData_(
                   cameraInfo.staticCameraCharacteristics, portraitModeTag)
               .length > 0;
  }
};

/**
 * Gets the data from Camera metadata by its tag.
 * @param {!cros.mojom.CameraMetadata} metadata Camera metadata from which to
 *     query the data.
 * @param {!cros.mojom.CameraMetadataTag} tag Camera metadata tag to query for.
 * @return {!Array<number>} An array containing elements whose types correspond
 *     to the format of input |tag|. If nothing is found, returns an empty
 *     array.
 * @private
 */
cca.mojo.getMetadataData_ = function(metadata, tag) {
  for (let i = 0; i < metadata.entryCount; i++) {
    const entry = metadata.entries[i];
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
          const numVal = Number(bigIntVal);
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
