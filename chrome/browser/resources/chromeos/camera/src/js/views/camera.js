// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
 * Creates the camera-view controller.
 * @param {cca.models.Gallery} model Model object.
 * @param {cca.device.DeviceInfoUpdater} infoUpdater
 * @param {cca.device.PhotoResolPreferrer} photoPreferrer
 * @param {cca.device.VideoConstraintsPreferrer} videoPreferrer
 * @constructor
 */
cca.views.Camera = function(
    model, infoUpdater, photoPreferrer, videoPreferrer) {
  cca.views.View.call(this, '#camera');

  /**
   * Gallery model used to save taken pictures.
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = model;

  /**
   * @type {cca.device.DeviceInfoUpdater}
   * @private
   */
  this.infoUpdater_ = infoUpdater;

  /**
   * Layout handler for the camera view.
   * @type {cca.views.camera.Layout}
   * @private
   */
  this.layout_ = new cca.views.camera.Layout();

  /**
   * Video preview for the camera.
   * @type {cca.views.camera.Preview}
   * @private
   */
  this.preview_ = new cca.views.camera.Preview(this.restart.bind(this));

  /**
   * Options for the camera.
   * @type {cca.views.camera.Options}
   * @private
   */
  this.options_ =
      new cca.views.camera.Options(infoUpdater, this.restart.bind(this));

  /**
   * @type {HTMLElement}
   */
  this.banner_ = document.querySelector('#banner');

  /**
   * @type {HTMLButtonElement}
   */
  this.bannerLearnMore_ = document.querySelector('#banner-learn-more');

  /**
   * Modes for the camera.
   * @type {cca.views.camera.Modes}
   * @private
   */
  this.modes_ = new cca.views.camera.Modes(
      photoPreferrer, videoPreferrer, this.restart.bind(this),
      async (result, filename) => {
        if (result.blob) {
          cca.metrics.log(
              cca.metrics.Type.CAPTURE, this.facingMode_, 0, result.resolution);
          try {
            await this.model_.savePhoto(result.blob, filename);
          } catch (e) {
            cca.toast.show('error_msg_save_file_failed');
            throw e;
          }
        }
      },
      async (result, filename) => {
        cca.metrics.log(
            cca.metrics.Type.CAPTURE, this.facingMode_, result.duration,
            result.resolution);
        try {
          await this.model_.saveVideo(result.chunkfile, filename);
        } catch (e) {
          cca.toast.show('error_msg_save_file_failed');
          throw e;
        }
      });

  /**
   * @type {?string}
   * @private
   */
  this.facingMode_ = null;

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
   * Promise for the operation that starts camera.
   * @type {Promise}
   * @private
   */
  this.started_ = null;

  /**
   * Promise for the current take of photo or recording.
   * @type {?Promise}
   * @private
   */
  this.take_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  document.querySelectorAll('#start-takephoto, #start-recordvideo')
      .forEach((btn) => btn.addEventListener('click', () => this.beginTake_()));

  document.querySelectorAll('#stop-takephoto, #stop-recordvideo')
      .forEach((btn) => btn.addEventListener('click', () => this.endTake_()));

  document.querySelector('#banner-close').addEventListener('click', () => {
    cca.util.animateCancel(this.banner_);
  });

  document.querySelector('#banner-learn-more').addEventListener('click', () => {
    cca.util.openHelp();
  });

  // Monitor the states to stop camera when locked/minimized.
  chrome.idle.onStateChanged.addListener((newState) => {
    this.locked_ = (newState == 'locked');
    if (this.locked_) {
      this.restart();
    }
  });
  chrome.app.window.current().onMinimized.addListener(() => this.restart());

  this.start_();
};

cca.views.Camera.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * @override
 */
cca.views.Camera.prototype.focus = function() {
  (async () => {
    const values = await new Promise((resolve) => {
      cca.proxy.browserProxy.localStorageGet(['isIntroShown'], resolve);
    });
    await this.started_;
    if (!values.isIntroShown) {
      cca.proxy.browserProxy.localStorageSet({isIntroShown: true});
      cca.util.animateOnce(this.banner_);
      this.bannerLearnMore_.focus({preventScroll: true});
    } else {
      // Avoid focusing invisible shutters.
      document.querySelectorAll('.shutter')
          .forEach((btn) => btn.offsetParent && btn.focus());
    }
  })();
};


/**
 * Begins to take photo or recording with the current options, e.g. timer.
 * @private
 */
cca.views.Camera.prototype.beginTake_ = function() {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
    return;
  }

  cca.state.set('taking', true);
  this.focus();  // Refocus the visible shutter button for ChromeVox.
  this.take_ = (async () => {
    try {
      await cca.views.camera.timertick.start();

      await this.modes_.current.startCapture();
    } catch (e) {
      if (e && e.message == 'cancel') {
        return;
      }
      console.error(e);
    } finally {
      this.take_ = null;
      cca.state.set('taking', false);
      this.focus();  // Refocus the visible shutter button for ChromeVox.
    }
  })();
};

/**
 * Ends the current take (or clears scheduled further takes if any.)
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.Camera.prototype.endTake_ = function() {
  cca.views.camera.timertick.cancel();
  this.modes_.current.stopCapture();
  return Promise.resolve(this.take_);
};

/**
 * @override
 */
cca.views.Camera.prototype.layout = function() {
  this.layout_.update();
};

/**
 * @override
 */
cca.views.Camera.prototype.handlingKey = function(key) {
  if (key == 'Ctrl-R') {
    cca.toast.show(this.preview_.toString());
    return true;
  }
  return false;
};

/**
 * Stops camera and tries to start camera stream again if possible.
 * @return {!Promise} Promise for the start-camera operation.
 */
cca.views.Camera.prototype.restart = function() {
  // Wait for ongoing 'start' and 'capture' done before restarting camera.
  return Promise
      .all([
        this.started_,
        Promise.resolve(!cca.state.get('taking') || this.endTake_()),
      ])
      .finally(() => {
        this.preview_.stop();
        this.start_();
        return this.started_;
      });
};

/**
 * Try start stream reconfiguration with specified device id.
 * @async
 * @param {?string} deviceId
 * @return {boolean} If found suitable stream and reconfigure successfully.
 */
cca.views.Camera.prototype.startWithDevice_ = async function(deviceId) {
  let supportedModes = null;
  for (const mode of this.modes_.getModeCandidates()) {
    try {
      if (!deviceId) {
        // Null for requesting default camera on HALv1.
        throw new Error('HALv1-api');
      }
      const previewRs =
          (await this.infoUpdater_.getDeviceResolutions(deviceId))[1];
      var resolCandidates =
          this.modes_.getResolutionCandidates(mode, deviceId, previewRs);
    } catch (e) {
      // Assume the exception here is thrown from error of HALv1 not support
      // resolution query, fallback to use v1 constraints-candidates.
      if (e.message == 'HALv1-api') {
        resolCandidates = this.modes_.getResolutionCandidatesV1(mode, deviceId);
      } else {
        throw e;
      }
    }
    for (const [captureResolution, previewCandidates] of resolCandidates) {
      if (supportedModes && !supportedModes.includes(mode)) {
        break;
      }
      for (const constraints of previewCandidates) {
        try {
          const stream = await cca.mojo.getUserMedia(deviceId, constraints);
          if (!supportedModes) {
            supportedModes = await this.modes_.getSupportedModes(stream);
            if (!supportedModes.includes(mode)) {
              stream.getTracks()[0].stop();
              break;
            }
          }
          await this.preview_.start(stream);
          this.facingMode_ =
              await this.options_.updateValues(constraints, stream);
          await this.modes_.updateModeSelectionUI(supportedModes);
          await this.modes_.updateMode(
              mode, stream, deviceId, captureResolution);
          cca.nav.close('warning', 'no-camera');
          return true;
        } catch (e) {
          this.preview_.stop();
          console.error(e);
        }
      }
    }
  }
  return false;
};

/**
 * Starts camera if the camera stream was stopped.
 * @private
 */
cca.views.Camera.prototype.start_ = function() {
  var suspend = this.locked_ || chrome.app.window.current().isMinimized();
  this.started_ =
      this.infoUpdater_
          .lockDeviceInfo(async () => {
            if (!suspend) {
              for (const id of await this.options_.videoDeviceIds()) {
                if (await this.startWithDevice_(id)) {
                  return;
                }
              }
            }
            throw new Error('suspend');
          })
          .catch((error) => {
            if (error && error.message != 'suspend') {
              console.error(error);
              cca.nav.open('warning', 'no-camera');
            }
            // Schedule to retry.
            if (this.retryStartTimeout_) {
              clearTimeout(this.retryStartTimeout_);
              this.retryStartTimeout_ = null;
            }
            this.retryStartTimeout_ = setTimeout(this.start_.bind(this), 100);
          });
};
