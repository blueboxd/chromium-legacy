// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import {assert} from '../chrome_util.js';
import {
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from '../device/constraints_preferrer.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../device/device_info_updater.js';
import * as metrics from '../metrics.js';
// eslint-disable-next-line no-unused-vars
import {ResultSaver} from '../models/result_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as nav from '../nav.js';
// eslint-disable-next-line no-unused-vars
import {PerfLogger} from '../perf.js';
import * as sound from '../sound.js';
import * as state from '../state.js';
import * as toast from '../toast.js';
import {
  Facing,
  Mode,
} from '../type.js';
import * as util from '../util.js';

import {Layout} from './camera/layout.js';
import {
  Modes,
  PhotoResult,  // eslint-disable-line no-unused-vars
  VideoResult,  // eslint-disable-line no-unused-vars
} from './camera/modes.js';
import {Options} from './camera/options.js';
import {Preview} from './camera/preview.js';
import * as timertick from './camera/timertick.js';
import {View, ViewName} from './view.js';

/**
 * Thrown when app window suspended during stream reconfiguration.
 */
class CameraSuspendedError extends Error {
  /**
   * @param {string=} message Error message.
   */
  constructor(message = 'Camera suspended.') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Camera-view controller.
 */
export class Camera extends View {
  /**
   * @param {!ResultSaver} resultSaver
   * @param {!DeviceInfoUpdater} infoUpdater
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   * @param {Mode} defaultMode
   * @param {!PerfLogger} perfLogger
   */
  constructor(
      resultSaver, infoUpdater, photoPreferrer, videoPreferrer, defaultMode,
      perfLogger) {
    super(ViewName.CAMERA);

    /**
     * @type {!DeviceInfoUpdater}
     * @private
     */
    this.infoUpdater_ = infoUpdater;

    /**
     * @type {!Mode}
     * @protected
     */
    this.defaultMode_ = defaultMode;

    /**
     * @type {!PerfLogger}
     * @private
     */
    this.perfLogger_ = perfLogger;

    /**
     * Layout handler for the camera view.
     * @type {!Layout}
     * @private
     */
    this.layout_ = new Layout();

    /**
     * Video preview for the camera.
     * @type {!Preview}
     * @private
     */
    this.preview_ = new Preview(this.start.bind(this));

    /**
     * Options for the camera.
     * @type {!Options}
     * @private
     */
    this.options_ = new Options(infoUpdater, this.start.bind(this));

    /**
     * @type {!ResultSaver}
     * @protected
     */
    this.resultSaver_ = resultSaver;

    /**
     * Device id of video device of active preview stream. Sets to null when
     * preview become inactive.
     * @type {?string}
     * @private
     */
    this.activeDeviceId_ = null;

    const createVideoSaver = async () => resultSaver.startSaveVideo();

    const playShutterEffect = () => {
      sound.play('#sound-shutter');
      util.animateOnce(this.preview_.video);
    };

    /**
     * Modes for the camera.
     * @type {Modes}
     * @private
     */
    this.modes_ = new Modes(
        this.defaultMode_, photoPreferrer, videoPreferrer,
        this.start.bind(this), this.doSavePhoto_.bind(this), createVideoSaver,
        this.doSaveVideo_.bind(this), playShutterEffect);

    /**
     * @type {!Facing}
     * @protected
     */
    this.facingMode_ = Facing.UNKNOWN;

    /**
     * @type {boolean}
     * @private
     */
    this.locked_ = false;

    /**
     * @type {?number}
     * @private
     */
    this.retryStartTimeout_ = null;

    /**
     * Promise for the camera stream configuration process. It's resolved to
     * boolean for whether the configuration is failed and kick out another
     * round of reconfiguration. Sets to null once the configuration is
     * completed.
     * @type {?Promise<boolean>}
     * @private
     */
    this.configuring_ = null;

    /**
     * Promise for the current take of photo or recording.
     * @type {?Promise}
     * @protected
     */
    this.take_ = null;

    document.querySelector('#start-takephoto')
        .addEventListener('click', () => this.beginTake_());

    document.querySelector('#stop-takephoto')
        .addEventListener('click', () => this.endTake_());

    const videoShutter = document.querySelector('#recordvideo');
    videoShutter.addEventListener('click', () => {
      if (!state.get(state.State.TAKING)) {
        this.beginTake_();
      } else {
        this.endTake_();
      }
    });

    // TODO(shik): Tune the timing for playing video shutter button
    // animation. Currently the |TAKING| state is ended when the file is saved.
    state.addObserver(state.State.TAKING, (taking) => {
      if (!state.get(Mode.VIDEO)) {
        return;
      }
      const label =
          taking ? 'record_video_stop_button' : 'record_video_start_button';
      videoShutter.setAttribute('i18n-label', label);
      videoShutter.setAttribute(
          'aria-label', browserProxy.getI18nMessage(label));
    });

    // Monitor the states to stop camera when locked/minimized.
    ChromeHelper.getInstance().addOnLockListener((isLocked) => {
      this.locked_ = isLocked;
      if (this.locked_) {
        this.start();
      }
    });
    chrome.app.window.current().onMinimized.addListener(() => this.start());

    document.addEventListener('visibilitychange', () => {
      const recording = state.get(state.State.TAKING) && state.get(Mode.VIDEO);
      if (this.isTabletBackground_() && !recording) {
        this.start();
      }
    });

    this.configuring_ = null;
  }

  /**
   * Initializes camera view.
   * @return {!Promise}
   */
  async initialize() {
    await ChromeHelper.getInstance().initTabletModeMonitor();
  }

  /**
   * @return {boolean} Returns if window is fully overlapped by other window in
   * both window mode or tablet mode.
   * @private
   */
  get isVisible_() {
    return document.visibilityState !== 'hidden';
  }

  /**
   * @return {boolean} Whether window is put to background in tablet mode.
   * @private
   */
  isTabletBackground_() {
    return ChromeHelper.getInstance().isTabletMode() && !this.isVisible_;
  }

  /**
   * Whether app window is suspended.
   * @return {boolean}
   */
  isSuspended() {
    return this.locked_ || chrome.app.window.current().isMinimized() ||
        state.get(state.State.SUSPEND) || this.isTabletBackground_();
  }

  /**
   * @override
   */
  focus() {
    // Avoid focusing invisible shutters.
    document.querySelectorAll('.shutter')
        .forEach((btn) => btn.offsetParent && btn.focus());
  }

  /**
   * Begins to take photo or recording with the current options, e.g. timer.
   * @return {?Promise} Promise resolved when take action completes. Returns
   *     null if CCA can't start take action.
   * @protected
   */
  beginTake_() {
    if (!state.get(state.State.STREAMING) || state.get(state.State.TAKING)) {
      return null;
    }

    state.set(state.State.TAKING, true);
    this.focus();  // Refocus the visible shutter button for ChromeVox.
    this.take_ = (async () => {
      let hasError = false;
      try {
        await timertick.start();
        await this.modes_.current.startCapture();
      } catch (e) {
        hasError = true;
        if (e && e.message === 'cancel') {
          return;
        }
        console.error(e);
      } finally {
        this.take_ = null;
        state.set(
            state.State.TAKING, false, {hasError, facing: this.facingMode_});
        this.focus();  // Refocus the visible shutter button for ChromeVox.
      }
    })();
    return this.take_;
  }

  /**
   * Ends the current take (or clears scheduled further takes if any.)
   * @return {!Promise} Promise for the operation.
   * @private
   */
  endTake_() {
    timertick.cancel();
    this.modes_.current.stopCapture();
    return Promise.resolve(this.take_);
  }

  /**
   * Handles captured photo result.
   * @param {!PhotoResult} result Captured photo result.
   * @param {string} name Name of the photo result to be saved as.
   * @return {!Promise} Promise for the operation.
   * @protected
   */
  async doSavePhoto_(result, name) {
    metrics.log(
        metrics.Type.CAPTURE, this.facingMode_, /* length= */ 0,
        result.resolution, metrics.IntentResultType.NOT_INTENT);
    try {
      await this.resultSaver_.savePhoto(result.blob, name);
    } catch (e) {
      toast.show('error_msg_save_file_failed');
      throw e;
    }
  }

  /**
   * Handles captured video result.
   * @param {!VideoResult} result Captured video result.
   * @param {string} name Name of the video result to be saved as.
   * @return {!Promise} Promise for the operation.
   * @protected
   */
  async doSaveVideo_(result, name) {
    metrics.log(
        metrics.Type.CAPTURE, this.facingMode_, result.duration,
        result.resolution, metrics.IntentResultType.NOT_INTENT);
    try {
      await this.resultSaver_.finishSaveVideo(result.videoSaver, name);
    } catch (e) {
      toast.show('error_msg_save_file_failed');
      throw e;
    }
  }

  /**
   * @override
   */
  layout() {
    this.layout_.update();
  }

  /**
   * @override
   */
  handlingKey(key) {
    if (key === 'Ctrl-R') {
      toast.show(this.preview_.toString());
      return true;
    }
    if ((key === 'AudioVolumeUp' || key === 'AudioVolumeDown') &&
        ChromeHelper.getInstance().isTabletMode() &&
        state.get(state.State.STREAMING)) {
      if (state.get(state.State.TAKING)) {
        this.endTake_();
      } else {
        this.beginTake_();
      }
      return true;
    }
    return false;
  }

  /**
   * Stops camera and tries to start camera stream again if possible.
   * @return {!Promise<boolean>} Promise resolved to whether start camera
   *     successfully.
   */
  async start() {
    // To prevent multiple callers enter this function at the same time, wait
    // until previous caller resets configuring to null.
    while (this.configuring_ !== null) {
      if (!await this.configuring_) {
        // Retry will be kicked out soon.
        return false;
      }
    }
    state.set(state.State.CAMERA_CONFIGURING, true);
    this.configuring_ = (async () => {
      try {
        if (state.get(state.State.TAKING)) {
          await this.endTake_();
        }
      } finally {
        this.preview_.stop();
      }
      return this.start_();
    })();
    return this.configuring_;
  }

  /**
   * Try start stream reconfiguration with specified mode and device id.
   * @param {?string} deviceId
   * @param {!Mode} mode
   * @return {!Promise<boolean>} If found suitable stream and reconfigure
   *     successfully.
   */
  async startWithMode_(deviceId, mode) {
    const deviceOperator = await DeviceOperator.getInstance();
    let resolCandidates = null;
    if (deviceOperator !== null) {
      if (deviceId !== null) {
        const previewRs =
            (await this.infoUpdater_.getDeviceResolutions(deviceId)).video;
        resolCandidates =
            this.modes_.getResolutionCandidates(mode, deviceId, previewRs);
      } else {
        console.error(
            'Null device id present on HALv3 device. Fallback to v1.');
      }
    }
    if (resolCandidates === null) {
      resolCandidates =
          await this.modes_.getResolutionCandidatesV1(mode, deviceId);
    }
    for (const {resolution: captureR, previewCandidates} of resolCandidates) {
      for (const constraints of previewCandidates) {
        if (this.isSuspended()) {
          throw new CameraSuspendedError();
        }
        try {
          if (deviceOperator !== null) {
            assert(deviceId !== null);
            const optConfigs =
                mode === Mode.VIDEO ? {} : {stillCaptureResolution: captureR};
            await deviceOperator.setStreamConfig(
                deviceId, constraints, optConfigs);
            await deviceOperator.setCaptureIntent(
                deviceId, this.modes_.getCaptureIntent(mode));
          }
          const stream = await navigator.mediaDevices.getUserMedia(constraints);
          await this.preview_.start(stream);
          this.facingMode_ = await this.options_.updateValues(stream);
          await this.modes_.updateModeSelectionUI(deviceId);
          await this.modes_.updateMode(
              mode, stream, this.facingMode_, deviceId, captureR);
          nav.close(ViewName.WARNING, 'no-camera');
          return true;
        } catch (e) {
          this.preview_.stop();
          console.error(e);
        }
      }
    }
    return false;
  }

  /**
   * Try start stream reconfiguration with specified device id.
   * @param {?string} deviceId
   * @return {!Promise<boolean>} If found suitable stream and reconfigure
   *     successfully.
   */
  async startWithDevice_(deviceId) {
    const supportedModes = await this.modes_.getSupportedModes(deviceId);
    const modes = this.modes_.getModeCandidates().filter(
        (m) => supportedModes.includes(m));
    for (const mode of modes) {
      if (await this.startWithMode_(deviceId, mode)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Starts camera configuration process.
   * @return {!Promise<boolean>} Resolved to boolean for whether the
   *     configuration is succeeded or kicks out another round of
   *     reconfiguration.
   * @private
   */
  async start_() {
    try {
      await this.infoUpdater_.lockDeviceInfo(async () => {
        if (!this.isSuspended()) {
          for (const id of await this.options_.videoDeviceIds()) {
            if (await this.startWithDevice_(id)) {
              // Make the different active camera announced by screen reader.
              const currentId = this.options_.currentDeviceId;
              assert(currentId !== null);
              if (currentId === this.activeDeviceId_) {
                return;
              }
              this.activeDeviceId_ = currentId;
              const info = await this.infoUpdater_.getDeviceInfo(currentId);
              if (info !== null) {
                toast.speak(browserProxy.getI18nMessage(
                    'status_msg_camera_switched', info.label));
              }
              return;
            }
          }
        }
        throw new CameraSuspendedError();
      });
      this.configuring_ = null;
      state.set(state.State.CAMERA_CONFIGURING, false);

      return true;
    } catch (error) {
      this.activeDeviceId_ = null;
      if (!(error instanceof CameraSuspendedError)) {
        console.error(error);
        nav.open(ViewName.WARNING, 'no-camera');
      }
      // Schedule to retry.
      if (this.retryStartTimeout_) {
        clearTimeout(this.retryStartTimeout_);
        this.retryStartTimeout_ = null;
      }
      this.retryStartTimeout_ = setTimeout(() => {
        this.configuring_ = this.start_();
      }, 100);

      this.perfLogger_.interrupt();
      return false;
    }
  }
}
