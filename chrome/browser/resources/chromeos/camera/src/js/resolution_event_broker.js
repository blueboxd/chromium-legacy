// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Broker for registering or notifying resolution related events.
 */
cca.ResolutionEventBroker = class {
  /** @public */
  constructor() {
    /**
     * Handler for requests of changing user-preferred resolution used in photo
     * taking.
     * @type {function(string, number, number)}
     * @private
     */
    this.photoChangePrefResolHandler_ = () => {};

    /**
     * Handler for requests of changing user-preferred resolution used in video
     * recording.
     * @type {function(string, number, number)}
     * @private
     */
    this.videoChangePrefResolHandler_ = () => {};

    /**
     * Listener for changes of preferred photo resolution used on particular
     * video device.
     * @type {function(string, number, number)}
     * @private
     */
    this.photoPrefResolChangeListener_ = () => {};

    /**
     * Listener for changes of preferred video resolution used on particular
     * video device.
     * @type {function(string, number, number)}
     * @private
     */
    this.videoPrefResolChangeListener_ = () => {};
  }

  /**
   * Registers handler for requests of changing user-preferred resolution used
   * in photo taking.
   * @param {function(string, number, number)} handler Called with device id of
   *     video device to be changed and width, height of new resolution.
   */
  registerChangePhotoPrefResolHandler(handler) {
    this.photoChangePrefResolHandler_ = handler;
  }

  /**
   * Registers handler for requests of changing user-preferred resolution used
   * in video recording.
   * @param {function(string, number, number)} handler Called with device id of
   *     video device to be changed and width, height of new resolution.
   */
  registerChangeVideoPrefResolHandler(handler) {
    this.videoChangePrefResolHandler_ = handler;
  }

  /**
   * Requests for changing user-preferred resolution used in photo taking.
   * @param {string} deviceId Device id of video device to be changed.
   * @param {number} width Change to resolution width.
   * @param {number} height Change to resolution height.
   */
  requestChangePhotoPrefResol(deviceId, width, height) {
    this.photoChangePrefResolHandler_(deviceId, width, height);
  }

  /**
   * Requests for changing user-preferred resolution used in video recording.
   * @param {string} deviceId Device id of video device to be changed.
   * @param {number} width Change to resolution width.
   * @param {number} height Change to resolution height.
   */
  requestChangeVideoPrefResol(deviceId, width, height) {
    this.videoChangePrefResolHandler_(deviceId, width, height);
  }

  /**
   * Adds listener for changes of preferred resolution used in taking photo on
   * particular video device.
   * @param {function(string, number, number)} listener Called with changed
   *     video device id and new preferred resolution width, height.
   */
  addPhotoPrefResolChangeListener(listener) {
    this.photoPrefResolChangeListener_ = listener;
  }

  /**
   * Adds listener for changes of preferred resolution used in video recording
   * on particular video device.
   * @param {function(string, number, number)} listener Called with changed
   *     video device id and new preferred resolution width, height.
   */
  addVideoPrefResolChangeListener(listener) {
    this.videoPrefResolChangeListener_ = listener;
  }

  /**
   * Notifies the change of preferred resolution used in photo taking on
   * particular video device.
   * @param {string} deviceId Device id of changed video device.
   * @param {number} width New resolution width.
   * @param {number} height New resolution height.
   */
  notifyPhotoPrefResolChange(deviceId, width, height) {
    this.photoPrefResolChangeListener_(deviceId, width, height);
  }

  /**
   * Notifies the change of preferred resolution used in video recording on
   * particular video device.
   * @param {string} deviceId Device id of changed video device.
   * @param {number} width New resolution width.
   * @param {number} height New resolution height.
   */
  notifyVideoPrefResolChange(deviceId, width, height) {
    this.videoPrefResolChangeListener_(deviceId, width, height);
  }
};
