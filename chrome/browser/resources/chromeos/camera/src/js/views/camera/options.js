// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Video device information queried from HALv3 mojo private API.
 * @param {MediaDeviceInfo} deviceInfo Information of the video device.
 * @param {cros.mojom.CameraFacing} facing Camera facing of the video device.
 * @param {ResolList} photoResols Supported available photo resolutions of the
 *     video device.
 * @param {Array<[number, number, number]>} videoResolFpses Supported available
 *     video resolutions and maximal capture fps of the video device.
 * @param {FpsRangeInfo} fpsRanges Supported fps ranges of the video device.
 */
cca.views.camera.Camera3DeviceInfo = function(
    deviceInfo, facing, photoResols, videoResolFpses, fpsRanges) {
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
   * @type {ResolList}
   * @public
   */
  this.photoResols = photoResols;

  /**
   * @type {ResolList}
   * @public
   */
  this.videoResols = [];

  /**
   * @type {MaxFpsInfo}
   * @public
   */
  this.videoMaxFps = {};

  /**
   * @type {FpsRangeInfo}
   * @public
   */
  this.fpsRanges = fpsRanges;

  // End of properties, seal the object.
  Object.seal(this);

  videoResolFpses.filter(([, , fps]) => fps >= 24).forEach(([w, h, fps]) => {
    this.videoResols.push([w, h]);
    this.videoMaxFps[[w, h]] = fps;
  });
};

/**
 * Creates a controller for the options of Camera view.
 * @param {cca.device.DeviceInfoUpdater} infoUpdater
 * @param {function()} doSwitchDevice Callback to trigger device switching.
 * @constructor
 */
cca.views.camera.Options = function(infoUpdater, doSwitchDevice) {
  /**
   * @type {cca.device.DeviceInfoUpdater}
   * @private
   */
  this.infoUpdater_ = infoUpdater;

  /**
   * @type {function()}
   * @private
   */
  this.doSwitchDevice_ = doSwitchDevice;

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleMic_ = document.querySelector('#toggle-mic');

  /**
   * @type {HTMLInputElement}
   * @private
   */
  this.toggleMirror_ = document.querySelector('#toggle-mirror');

  /**
   * Device id of the camera device currently used or selected.
   * @type {?string}
   * @private
   */
  this.videoDeviceId_ = null;

  /**
   * Whether list of video devices is being refreshed now.
   * @type {boolean}
   * @private
   */
  this.refreshingVideoDeviceIds_ = false;

  /**
   * List of available video devices.
   * @type {Promise<!Array<MediaDeviceInfo>>}
   * @private
   */
  this.videoDevices_ = null;

  /**
   * Promise for querying Camera3DeviceInfo of all available video devices from
   * mojo private API.
   * @type {Promise<!Array<Camera3DeviceInfo>>}
   * @private
   */
  this.devicesPrivateInfo_ = null;

  /**
   * Whether the current device is HALv1 and lacks facing configuration.
   * get facing information.
   * @type {?boolean}
   * private
   */
  this.isV1NoFacingConfig_ = null;

  /**
   * Mirroring set per device.
   * @type {Object}
   * @private
   */
  this.mirroringToggles_ = {};

  /**
   * Current audio track in use.
   * @type {MediaStreamTrack}
   * @private
   */
  this.audioTrack_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  [['#switch-device', () => this.switchDevice_()],
   ['#toggle-grid', () => this.animatePreviewGrid_()],
   ['#open-settings', () => cca.nav.open('settings')],
  ]
      .forEach(
          ([selector, fn]) =>
              document.querySelector(selector).addEventListener('click', fn));

  this.toggleMic_.addEventListener('click', () => this.updateAudioByMic_());
  this.toggleMirror_.addEventListener('click', () => this.saveMirroring_());

  // Restore saved mirroring states per video device.
  cca.proxy.browserProxy.localStorageGet(
      {mirroringToggles: {}},
      (values) => this.mirroringToggles_ = values.mirroringToggles);
  // Remove the deprecated values.
  cca.proxy.browserProxy.localStorageRemove(
      ['effectIndex', 'toggleMulti', 'toggleMirror']);

  this.infoUpdater_.addDeviceChangeListener(async (updater) => {
    cca.state.set('multi-camera', (await updater.getDevicesInfo()).length >= 2);
  });
};

/**
 * Switches to the next available camera device.
 * @private
 */
cca.views.camera.Options.prototype.switchDevice_ = async function() {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
    return;
  }
  const devices = await this.infoUpdater_.getDevicesInfo();
  cca.util.animateOnce(document.querySelector('#switch-device'));
  var index =
      devices.findIndex((entry) => entry.deviceId == this.videoDeviceId_);
  if (index == -1) {
    index = 0;
  }
  if (devices.length > 0) {
    index = (index + 1) % devices.length;
    this.videoDeviceId_ = devices[index].deviceId;
  }
  await this.doSwitchDevice_();
  // Make the active camera announced by screen reader.
  var found = devices.find((entry) => entry.deviceId == this.videoDeviceId_);
  if (found) {
    cca.toast.speak(
        chrome.i18n.getMessage('status_msg_camera_switched', found.label));
  }
};

/**
 * Animates the preview grid.
 * @private
 */
cca.views.camera.Options.prototype.animatePreviewGrid_ = function() {
  Array.from(document.querySelector('#preview-grid').children).forEach(
      (grid) => cca.util.animateOnce(grid));
};

/**
 * Updates the options' values for the current constraints and stream.
 * @param {Object} constraints Current stream constraints in use.
 * @param {MediaStream} stream Current Stream in use.
 * @return {?string} Facing-mode in use.
 */
cca.views.camera.Options.prototype.updateValues =
    async function(constraints, stream) {
  var track = stream.getVideoTracks()[0];
  var trackSettings = track.getSettings && track.getSettings();
  let facingMode = trackSettings && trackSettings.facingMode;
  if (this.isV1NoFacingConfig_ === null) {
    // Because the facing mode of external camera will be set to undefined on
    // all devices, to distinguish HALv1 device without facing configuration,
    // assume the first opened camera is built-in camera. Device without facing
    // configuration won't set facing of built-in cameras. Also if HALv1 device
    // with facing configuration opened external camera first after CCA launched
    // the logic here may misjudge it as this category.
    this.isV1NoFacingConfig_ = facingMode === undefined;
  }
  facingMode = this.isV1NoFacingConfig_ ? null : facingMode || 'external';
  this.updateVideoDeviceId_(constraints, trackSettings);
  this.updateMirroring_(facingMode);
  this.audioTrack_ = stream.getAudioTracks()[0];
  this.updateAudioByMic_();
  return facingMode;
};

/**
 * Updates the video device id by the new stream.
 * @param {Object} constraints Stream constraints in use.
 * @param {MediaTrackSettings} trackSettings Video track settings in use.
 * @private
 */
cca.views.camera.Options.prototype.updateVideoDeviceId_ = function(
    constraints, trackSettings) {
  if (constraints.video.deviceId) {
    // For non-default cameras fetch the deviceId from constraints.
    // Works on all supported Chrome versions.
    this.videoDeviceId_ = constraints.video.deviceId.exact;
  } else {
    // For default camera, obtain the deviceId from settings, which is
    // a feature available only from 59. For older Chrome versions,
    // it's impossible to detect the device id. As a result, if the
    // default camera was changed to rear in chrome://settings, then
    // toggling the camera may not work when pressed for the first time
    // (the same camera would be opened).
    this.videoDeviceId_ = trackSettings && trackSettings.deviceId || null;
  }
};

/**
 * Updates mirroring for a new stream.
 * @param {string} facingMode Facing-mode of the stream.
 * @private
 */
cca.views.camera.Options.prototype.updateMirroring_ = function(facingMode) {
  // Update mirroring by detected facing-mode. Enable mirroring by default if
  // facing-mode isn't available.
  var enabled = facingMode ? facingMode == 'user' : true;

  // Override mirroring only if mirroring was toggled manually.
  if (this.videoDeviceId_ in this.mirroringToggles_) {
    enabled = this.mirroringToggles_[this.videoDeviceId_];
  }
  this.toggleMirror_.toggleChecked(enabled);
};

/**
 * Saves the toggled mirror state for the current video device.
 * @private
 */
cca.views.camera.Options.prototype.saveMirroring_ = function() {
  this.mirroringToggles_[this.videoDeviceId_] = this.toggleMirror_.checked;
  cca.proxy.browserProxy.localStorageSet(
      {mirroringToggles: this.mirroringToggles_});
};

/**
 * Enables/disables the current audio track by the microphone option.
 * @private
 */
cca.views.camera.Options.prototype.updateAudioByMic_ = function() {
  if (this.audioTrack_) {
    this.audioTrack_.enabled = this.toggleMic_.checked;
  }
};

/**
 * Updates list of available video devices when changed, including the UI.
 * Does nothing if refreshing is already in progress.
 * @private
 */
cca.views.camera.Options.prototype.maybeRefreshVideoDeviceIds_ = function() {
  if (this.refreshingVideoDeviceIds_) {
    return;
  }
  this.refreshingVideoDeviceIds_ = true;

  this.videoDevices_ = navigator.mediaDevices.enumerateDevices().then(
      (devices) => devices.filter((device) => device.kind == 'videoinput'));

  var multi = false;
  this.videoDevices_.then((devices) => {
    multi = devices.length >= 2;
  }).catch(console.error).finally(() => {
    cca.state.set('multi-camera', multi);
    this.refreshingVideoDeviceIds_ = false;
  });

  this.devicesPrivateInfo_ = (async () => {
    const devices = await this.videoDevices_;
    try {
      var privateInfos = await Promise.all(devices.map((d) => Promise.all([
        d,
        cca.mojo.getCameraFacing(d.deviceId),
        cca.mojo.getPhotoResolutions(d.deviceId),
        cca.mojo.getVideoConfigs(d.deviceId),
        cca.mojo.getSupportedFpsRanges(d.deviceId),
      ])));
    } catch (e) {
      cca.state.set('no-resolution-settings', true);
      throw new Error('HALv1-api');
    }
    return privateInfos.map(
        (info) => new cca.views.camera.Camera3DeviceInfo(...info));
  })();

  (async () => {
    try {
      var devicesPrivateInfo = await this.devicesPrivateInfo_;
    } catch (e) {
      if (e.message == 'HALv1-api') {
        return;
      }
      throw e;
    }
    let frontSetting = null;
    let backSetting = null;
    let externalSettings = [];
    devicesPrivateInfo.forEach((info) => {
      const setting = [info.deviceId, info.photoResols, info.videoResols];
      switch (info.facing) {
        case cros.mojom.CameraFacing.CAMERA_FACING_FRONT:
          frontSetting = setting;
          break;
        case cros.mojom.CameraFacing.CAMERA_FACING_BACK:
          backSetting = setting;
          break;
        case cros.mojom.CameraFacing.CAMERA_FACING_EXTERNAL:
          externalSettings.push(setting);
          break;
        default:
          console.error(`Ignore device of unknown facing: ${info.facing}`);
      }
    });
    this.photoResolPreferrer_.updateResolutions(
        frontSetting && [frontSetting[0], frontSetting[1]],
        backSetting && [backSetting[0], backSetting[1]],
        externalSettings.map(([deviceId, photoRs]) => [deviceId, photoRs]));
    this.videoPreferrer_.updateResolutions(
        frontSetting && [frontSetting[0], frontSetting[2]],
        backSetting && [backSetting[0], backSetting[2]],
        externalSettings.map(([deviceId, , videoRs]) => [deviceId, videoRs]));

    this.videoPreferrer_.updateFpses(devicesPrivateInfo.map(
        (info) => [info.deviceId, info.videoMaxFps, info.fpsRanges]));
  })();
};

/**
 * Gets the video device ids sorted by preference.
 * @async
 * @return {Array<?string>} May contain null for user facing camera on HALv1
 *     devices.
 * @throws {Error} Throws exception for no available video devices.
 */
cca.views.camera.Options.prototype.videoDeviceIds = async function() {
  const camera3Info = await this.infoUpdater_.getCamera3DevicesInfo();

  let facings = null;
  if (camera3Info) {
    var devices = camera3Info;
    facings = camera3Info.reduce(
        (facings, info) =>
            Object.assign(facings, {[info.deviceId]: info.facing}),
        {});
    devices = camera3Info;
    this.isV1NoFacingConfig_ = false;
  } else {
    devices = await this.infoUpdater_.getDevicesInfo();
  }
  // Put the selected video device id first.
  var sorted = devices.map((device) => device.deviceId).sort((a, b) => {
    if (a == b) {
      return 0;
    }
    if (this.videoDeviceId_ ?
            a === this.videoDeviceId_ :
            (facings &&
             facings[a] === cros.mojom.CameraFacing.CAMERA_FACING_FRONT)) {
      return -1;
    }
    return 1;
  });
  // Prepended 'null' deviceId means the system default camera on HALv1
  // device. Add it only when the app is launched (no video-device-id set).
  if (!facings && this.videoDeviceId_ === null) {
    sorted.unshift(null);
  }
  return sorted;
};
