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
 * Connector for using CrOS mojo interface.
 */
cca.mojo.MojoConnector = class {
  /** @public */
  constructor() {
    /**
     * An interface remote that used to construct the mojo interface.
     * @type {cros.mojom.CameraAppDeviceProviderRemote}
     * @private
     */
    this.deviceProvider_ = cros.mojom.CameraAppDeviceProvider.getRemote();

    /**
     * The device operator that could operates video capture device through mojo
     * interface.
     * @type {cca.mojo.DeviceOperator}
     * @private
     */
    this.deviceOperator_ = null;

    /**
     * Promise that indicates if the device operator is ready.
     * @type {!Promise}
     * @private
     */
    this.isDeviceOperatorReady_ = this.initDeviceOperator();
  }

  /**
   * Enumerates the devices and construct mojo connection for each of them.
   * @throws {Error} Thrown when there is any unexpected errors. Note that if it
   *     fails since it runs on camera hal v1 stack, no error will be thrown.
   * @private
   */
  async initDeviceOperator() {
    const devices = await navigator.mediaDevices.enumerateDevices();
    const videoDevices = devices.filter(({kind}) => kind === 'videoinput');
    const connectedDevices = new Map();
    for (const videoDevice of videoDevices) {
      const result =
          await this.deviceProvider_.getCameraAppDevice(videoDevice.deviceId);
      switch (result.status) {
        case cros.mojom.GetCameraAppDeviceStatus.SUCCESS:
          connectedDevices.set(videoDevice.deviceId, result.device);
          break;
        case cros.mojom.GetCameraAppDeviceStatus.ERROR_NON_V3:
          return;
        default:
          throw new Error(`Failed to connect to: ${
              videoDevice.deviceId}, error: ${result.status}`);
      }
    }
    this.deviceOperator_ = new cca.mojo.DeviceOperator(connectedDevices);
  }


  /**
   * Resets the mojo connection. Once it is reset, the connector should be
   * initialized again before being used.
   */
  async reset() {
    this.deviceOperator_ = null;
    this.isDeviceOperatorReady_ = this.initDeviceOperator();
  }

  /**
   * Gets the device operator.
   * @return {!Promise<?cca.mojo.DeviceOperator>} The video capture device
   *     operator. For non-v3 devices, it returns null.
   */
  async getDeviceOperator() {
    try {
      await this.isDeviceOperatorReady_;
    } catch (e) {
      console.error(e);
      return null;
    }
    return this.deviceOperator_;
  }
};
