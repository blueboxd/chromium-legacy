// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for device.
 */
cca.device = cca.device || {};

/* eslint-disable no-unused-vars */

/**
 * A list of resolutions represented as [width, height].
 * @typedef {Array<!Array<number>>}
 */
var ResolList;

/**
 * Map of all available resolution to its maximal supported capture fps. The key
 * of the map is the resolution formatted as [width, height] and the
 * corresponding value is the maximal capture fps under that resolution.
 * @typedef {Object<!Array<number>, number>}
 */
var MaxFpsInfo;

/**
 * List of supported capture fps ranges, each of range format as [minimal fps,
 * maximal fps].
 * @typedef {Array<!Array<number>>}
 */
var FpsRangeInfo;

/* eslint-enable no-unused-vars */

/**
 * Video device information queried from HALv3 mojo private API.
 */
cca.device.Camera3DeviceInfo = class {
  /**
   * @public
   * @param {!MediaDeviceInfo} deviceInfo Information of the video device.
   * @param {!cros.mojom.CameraFacing} facing Camera facing of the video device.
   * @param {!ResolList} photoResols Supported available photo resolutions of
   *     the video device.
   * @param {!Array<!Array<number>>} videoResolFpses Supported available video
   *     resolutions and maximal capture fps of the video device. Each available
   *     option represents as [width, height, max_fps] format item in the array.
   * @param {!FpsRangeInfo} fpsRanges Supported fps ranges of the video device.
   */
  constructor(deviceInfo, facing, photoResols, videoResolFpses, fpsRanges) {
    /**
     * @type {string}
     * @public
     */
    this.deviceId = deviceInfo.deviceId;

    /**
     * @type {cros.mojom.CameraFacing}
     * @public
     */
    this.facing = facing;

    /**
     * @type {!ResolList}
     * @public
     */
    this.photoResols = photoResols;

    /**
     * @type {!ResolList}
     * @public
     */
    this.videoResols = [];

    /**
     * @type {!MaxFpsInfo}
     * @public
     */
    this.videoMaxFps = {};

    /**
     * @type {!FpsRangeInfo}
     * @public
     */
    this.fpsRanges = fpsRanges;

    videoResolFpses.filter(([, , fps]) => fps >= 24).forEach(([w, h, fps]) => {
      this.videoResols.push([w, h]);
      this.videoMaxFps[[w, h]] = fps;
    });
  }
};
