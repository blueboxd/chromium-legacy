// Copyright 2019 The Chromium Authors. All rights reserved.
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
 * Controller for managing preference of capture settings and generating a list
 * of stream constraints-candidates sorted by user preference.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doReconfigureStream Trigger stream reconfiguration to
 *     reflect changes in user preferred settings.
 * @constructor
 */
cca.views.camera.ConstraintsPreferrer = function(
    resolBroker, doReconfigureStream) {
  /**
   * @type {cca.ResolutionEventBroker}
   * @protected
   */
  this.resolBroker_ = resolBroker;

  /**
   * @type {function()}
   * @protected
   */
  this.doReconfigureStream_ = doReconfigureStream;

  /**
   * Object saving resolution preference that each of its key as device id and
   * value to be preferred width, height of resolution of that video device.
   * @type {?Object<string, {width: number, height: number}>}
   * @protected
   */
  this.prefResolution_ = null;

  /**
   * Device id of currently working video device.
   * @type {?string}
   * @protected
   */
  this.deviceId_ = null;

  /**
   * Object of device id as its key and all of available capture resolutions
   * supported by that video device as its value.
   * @type {Object<string, ResolList>}
   * @protected
   */
  this.deviceResolutions_ = null;
};

/**
 * Updates resolution preference based on newly updated available resolutions.
 * @param {?[string, ResolList]} frontResolutions Device id and available
 *     resolutions of front camera.
 * @param {?[string, ResolList]} backResolutions Device id and available
 *     resolutions of back camera.
 * @param {Array<[string, ResolList]>} externalResolutions Device ids and
 *     available resolutions of all external cameras.
 * @public
 */
cca.views.camera.ConstraintsPreferrer.prototype.updateResolutions = function(
    frontResolutions, backResolutions, externalResolutions) {};

/**
 * Updates values according to currently working video device and capture
 * settings.
 * @param {string} deviceId Device id of video device to be updated.
 * @param {MediaStream} stream Currently active preview stream.
 * @param {number} width Width of resolution to be updated to.
 * @param {number} height Height of resolution to be updated to.
 * @public
 */
cca.views.camera.ConstraintsPreferrer.prototype.updateValues = function(
    deviceId, stream, width, height) {};

/**
 * Gets all available candidates for capturing under this controller and its
 * corresponding preview constraints for the specified video device. Returned
 * resolutions and constraints candidates are both sorted in desired trying
 * order.
 * @param {string} deviceId Device id of video device.
 * @param {ResolList} previewResolutions Available preview resolutions for the
 *     video device.
 * @return {Array<[[number, number], Array<Object>]>} Result capture resolution
 *     width, height and constraints-candidates for its preview.
 * @public
 */
cca.views.camera.ConstraintsPreferrer.prototype.getSortedCandidates = function(
    deviceId, previewResolutions) {
  return null;
};

/**
 * Controller for handling video resolution preference.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doReconfigureStream
 * @constructor
 */
cca.views.camera.VideoConstraintsPreferrer = function(
    resolBroker, doReconfigureStream) {
  cca.views.camera.ConstraintsPreferrer.call(
      this, resolBroker, doReconfigureStream);

  /**
   * Object saving information of supported constant fps. Each of its key as
   * device id and value as an object mapping from resolution to all constant
   * fps options supported by that resolution.
   * @type {Object<string, Object<[number, number], Array<number>>>}
   * @private
   */
  this.constFpsInfo_ = {};

  /**
   * Object saving fps preference that each of its key as device id and value as
   * an object mapping from resolution to preferred constant fps for that
   * resolution.
   * @type {Object<string, Object<[number, number], number>>}
   */
  this.prefFpses_ = {};

  /**
   * @type {HTMLButtonElement}
   */
  this.toggleFps_ = document.querySelector('#toggle-fps');

  /**
   * Currently in used recording resolution.
   * @type {?[number, number]}
   * @protected
   */
  this.resolution_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  // Restore saved preferred recording fps per video device per resolution.
  cca.proxy.browserProxy.localStorageGet(
      {deviceVideoFps: {}},
      (values) => this.prefFpses_ = values.deviceVideoFps);

  // Restore saved preferred video resolution per video device.
  cca.proxy.browserProxy.localStorageGet(
      {deviceVideoResolution: {}},
      (values) => this.prefResolution_ = values.deviceVideoResolution);

  this.resolBroker_.registerChangeVideoPrefResolHandler(
      (deviceId, width, height) => {
        this.prefResolution_[deviceId] = {width, height};
        cca.proxy.browserProxy.localStorageSet(
            {deviceVideoResolution: this.prefResolution_});
        if (cca.state.get('video-mode') && deviceId == this.deviceId_) {
          this.doReconfigureStream_();
        } else {
          this.resolBroker_.notifyVideoPrefResolChange(deviceId, width, height);
        }
      });

  this.toggleFps_.addEventListener('click', (event) => {
    if (!cca.state.get('streaming') || cca.state.get('taking')) {
      event.preventDefault();
    }
  });
  this.toggleFps_.addEventListener('change', (event) => {
    this.setPreferredConstFps_(
        this.deviceId_, ...this.resolution_, this.toggleFps_.checked ? 60 : 30);
    this.doReconfigureStream_();
  });
};

cca.views.camera.VideoConstraintsPreferrer.prototype = {
  __proto__: cca.views.camera.ConstraintsPreferrer.prototype,
};

/**
 * All supported constant fps options of video recording.
 * @type {Array<number>}
 * @const
 */
cca.views.camera.SUPPORTED_CONSTANT_FPS = [30, 60];

/**
 * Sets the preferred fps used in video recording for particular video device
 * and particular resolution.
 * @param {string} deviceId Device id of video device to be set with.
 * @param {number} width Resolution width to be set with.
 * @param {number} height Resolution height to be set with.
 * @param {number} prefFps Preferred fps to be set with.
 * @private
 */
cca.views.camera.VideoConstraintsPreferrer.prototype.setPreferredConstFps_ =
    function(deviceId, width, height, prefFps) {
  if (!cca.views.camera.SUPPORTED_CONSTANT_FPS.includes(prefFps)) {
    return;
  }
  this.toggleFps_.checked = prefFps === 60;
  cca.views.camera.SUPPORTED_CONSTANT_FPS.forEach(
      (fps) => cca.state.set(`_${fps}fps`, fps == prefFps));
  this.prefFpses_[deviceId] = this.prefFpses_[deviceId] || {};
  this.prefFpses_[deviceId][[width, height]] = prefFps;
  cca.proxy.browserProxy.localStorageSet({deviceVideoFps: this.prefFpses_});
};

/**
 * @param {?[string, ResolList]} frontResolutions
 * @param {?[string, ResolList]} backResolutions
 * @param {Array<[string, ResolList]>} externalResolutions
 * @override
 * @public
 */
cca.views.camera.VideoConstraintsPreferrer.prototype.updateResolutions =
    function(frontResolutions, backResolutions, externalResolutions) {
  this.deviceResolutions_ = {};

  const toDeviceIdResols = (deviceId, resolutions) => {
    this.deviceResolutions_[deviceId] = resolutions;
    let {width = -1, height = -1} = this.prefResolution_[deviceId] || {};
    if (!resolutions.find(([w, h]) => w == width && h == height)) {
      [width, height] = resolutions.reduce(
          (maxR, R) => (maxR[0] * maxR[1] < R[0] * R[1] ? R : maxR), [0, 0]);
    }
    this.prefResolution_[deviceId] = {width, height};
    return [deviceId, width, height, resolutions];
  };

  this.resolBroker_.notifyVideoResolChange(
      frontResolutions && toDeviceIdResols(...frontResolutions),
      backResolutions && toDeviceIdResols(...backResolutions),
      externalResolutions.map((ext) => toDeviceIdResols(...ext)));
  cca.proxy.browserProxy.localStorageSet(
      {deviceVideoResolution: this.prefResolution_});
};

/**
 * Updates information of supported fps for all video devices.
 * @param {Array<[string, MaxFpsInfo, FpsRangeInfo]>} deviceFpsInfo Video device
 *     id and max fps for all of its supported video resolutions and supported
 *     fps ranges of that device.
 * @public
 */
cca.views.camera.VideoConstraintsPreferrer.prototype.updateFpses = function(
    deviceFpsInfo) {
  this.constFpsInfo_ = {};
  for (const [deviceId, maxFpses, fpsRanges] of deviceFpsInfo) {
    const constFpses = cca.views.camera.SUPPORTED_CONSTANT_FPS.filter((fps) => {
      return fpsRanges.find(
                 ([minFps, maxFps]) => minFps === fps && maxFps === fps) !==
          undefined;
    });
    const fpsInfo = {};
    for (const r of Object.keys(maxFpses)) {
      fpsInfo[r] = constFpses.filter((fps) => fps <= maxFpses[r]);
    }
    this.constFpsInfo_[deviceId] = fpsInfo;
  }
};

/**
 * @param {string} deviceId
 * @param {MediaStream} stream
 * @param {number} width
 * @param {number} height
 * @public
 * @override
 */
cca.views.camera.VideoConstraintsPreferrer.prototype.updateValues = function(
    deviceId, stream, width, height) {
  this.deviceId_ = deviceId;
  this.resolution_ = [width, height];
  this.prefResolution_[deviceId] = {width, height};
  cca.proxy.browserProxy.localStorageSet(
      {deviceVideoResolution: this.prefResolution_});
  this.resolBroker_.notifyVideoPrefResolChange(deviceId, width, height);

  const fps = stream.getVideoTracks()[0].getSettings().frameRate;
  const availableFpses = this.constFpsInfo_[deviceId][[width, height]];
  if (availableFpses.includes(fps)) {
    this.setPreferredConstFps_(deviceId, width, height, fps);
  }
  cca.state.set('multi-fps', availableFpses.length > 1);
};

/**
 * @param {string} deviceId
 * @param {ResolList} previewResolutions
 * @return {Array<[[number, number], Array<Object>]>}
 * @public
 * @override
 */
cca.views.camera.VideoConstraintsPreferrer.prototype.getSortedCandidates =
    function(deviceId, previewResolutions) {
  // Due to the limitation of MediaStream API, preview stream is used directly
  // to do video recording.
  const prefR = this.prefResolution_[deviceId] || {width: 0, height: -1};
  const sortPrefResol = ([w, h], [w2, h2]) => {
    if (w == w2 && h == h2) {
      return 0;
    }
    // Exactly the preferred resolution.
    if (w == prefR.width && h == prefR.height) {
      return -1;
    }
    if (w2 == prefR.width && h2 == prefR.height) {
      return 1;
    }
    // Aspect ratio same as preferred resolution.
    if (w * h2 != w2 * h) {
      if (w * prefR.height == prefR.width * h) {
        return -1;
      }
      if (w2 * prefR.height == prefR.width * h2) {
        return 1;
      }
    }
    return w2 * h2 - w * h;
  };

  // Maps specified video resolution width, height to tuple of width, height and
  // all supported constant fps under that resolution or null fps for not
  // support multiple constant fps options. The resolution-fps tuple are sorted
  // by user preference of constant fps.
  const getFpses = (r) => {
    let constFpses = [null];
    if (this.constFpsInfo_[deviceId][r].includes(30) &&
        this.constFpsInfo_[deviceId][r].includes(60)) {
      const prefFps =
          this.prefFpses_[deviceId] && this.prefFpses_[deviceId][r] || 30;
      constFpses = prefFps == 30 ? [30, 60] : [60, 30];
    }
    return constFpses.map((fps) => [...r, fps]);
  };

  const toConstraints = (width, height, fps) => ({
    audio: true,
    video: {
      deviceId: {exact: deviceId},
      frameRate: fps ? {exact: fps} : {min: 24},
      width,
      height,
    },
  });

  return [...this.deviceResolutions_[deviceId]]
      .sort(sortPrefResol)
      .flatMap(getFpses)
      .map(([width, height, fps]) => ([
             [width, height],
             [toConstraints(width, height, fps)],
           ]));
};

/**
 * Controller for handling photo resolution preference.
 * @param {cca.ResolutionEventBroker} resolBroker
 * @param {function()} doReconfigureStream
 * @constructor
 */
cca.views.camera.PhotoResolPreferrer = function(
    resolBroker, doReconfigureStream) {
  cca.views.camera.ConstraintsPreferrer.call(
      this, resolBroker, doReconfigureStream);

  // End of properties, seal the object.
  Object.seal(this);

  // Restore saved preferred photo resolution per video device.
  cca.proxy.browserProxy.localStorageGet(
      {devicePhotoResolution: {}},
      (values) => this.prefResolution_ = values.devicePhotoResolution);

  this.resolBroker_.registerChangePhotoPrefResolHandler(
      (deviceId, width, height) => {
        this.prefResolution_[deviceId] = {width, height};
        cca.proxy.browserProxy.localStorageSet(
            {devicePhotoResolution: this.prefResolution_});
        if (!cca.state.get('video-mode') && deviceId == this.deviceId_) {
          this.doReconfigureStream_();
        } else {
          this.resolBroker_.notifyPhotoPrefResolChange(deviceId, width, height);
        }
      });
};

cca.views.camera.PhotoResolPreferrer.prototype = {
  __proto__: cca.views.camera.ConstraintsPreferrer.prototype,
};

/**
 * @param {?[string, ResolList]} frontResolutions
 * @param {?[string, ResolList]} backResolutions
 * @param {Array<[string, ResolList]>} externalResolutions
 * @public
 * @override
 */
cca.views.camera.PhotoResolPreferrer.prototype.updateResolutions = function(
    frontResolutions, backResolutions, externalResolutions) {
  this.deviceResolutions_ = {};

  const toDeviceIdResols = (deviceId, resolutions) => {
    this.deviceResolutions_[deviceId] = resolutions;
    let {width = -1, height = -1} = this.prefResolution_[deviceId] || {};
    if (!resolutions.find(([w, h]) => w == width && h == height)) {
      [width, height] = resolutions.reduce(
          (maxR, R) => (maxR[0] * maxR[1] < R[0] * R[1] ? R : maxR), [0, 0]);
    }
    this.prefResolution_[deviceId] = {width, height};
    return [deviceId, width, height, resolutions];
  };

  this.resolBroker_.notifyPhotoResolChange(
      frontResolutions && toDeviceIdResols(...frontResolutions),
      backResolutions && toDeviceIdResols(...backResolutions),
      externalResolutions.map((ext) => toDeviceIdResols(...ext)));
  cca.proxy.browserProxy.localStorageSet(
      {devicePhotoResolution: this.prefResolution_});
};

/**
 * @param {string} deviceId
 * @param {MediaStream} stream
 * @param {number} width
 * @param {number} height
 * @public
 * @override
 */
cca.views.camera.PhotoResolPreferrer.prototype.updateValues = function(
    deviceId, stream, width, height) {
  this.deviceId_ = deviceId;
  this.prefResolution_[deviceId] = {width, height};
  cca.proxy.browserProxy.localStorageSet(
      {devicePhotoResolution: this.prefResolution_});
  this.resolBroker_.notifyPhotoPrefResolChange(deviceId, width, height);
};

/**
 * Finds and pairs photo resolutions and preview resolutions with the same
 * aspect ratio.
 * @param {ResolList} captureResolutions Available photo capturing resolutions.
 * @param {ResolList} previewResolutions Available preview resolutions.
 * @return {Array<[ResolList, ResolList]>} Each item of returned array is a pair
 *     of capture and preview resolutions of same aspect ratio.
 */
cca.views.camera.PhotoResolPreferrer.prototype.pairCapturePreviewResolutions_ =
    function(captureResolutions, previewResolutions) {
  const toAspectRatio = (w, h) => (w / h).toFixed(4);
  const previewRatios = previewResolutions.reduce((rs, [w, h]) => {
    const key = toAspectRatio(w, h);
    rs[key] = rs[key] || [];
    rs[key].push([w, h]);
    return rs;
  }, {});
  const captureRatios = captureResolutions.reduce((rs, [w, h]) => {
    const key = toAspectRatio(w, h);
    if (key in previewRatios) {
      rs[key] = rs[key] || [];
      rs[key].push([w, h]);
    }
    return rs;
  }, {});
  return Object.entries(captureRatios)
      .map(([aspectRatio,
             captureRs]) => [captureRs, previewRatios[aspectRatio]]);
};

/**
 * @param {string} deviceId
 * @param {ResolList} previewResolutions
 * @return {Array<[[number, number], Array<Object>]>}
 * @public
 * @override
 */
cca.views.camera.PhotoResolPreferrer.prototype.getSortedCandidates = function(
    deviceId, previewResolutions) {
  const photoResolutions = this.deviceResolutions_[deviceId];
  const prefR = this.prefResolution_[deviceId] || {width: 0, height: -1};
  return this
      .pairCapturePreviewResolutions_(photoResolutions, previewResolutions)
      .map(([captureRs, previewRs]) => {
        if (captureRs.some(([w, h]) => w == prefR.width && h == prefR.height)) {
          var captureR = [prefR.width, prefR.height];
        } else {
          var captureR = captureRs.reduce(
              (captureR, r) => (r[0] > captureR[0] ? r : captureR), [0, -1]);
        }

        const candidates = [...previewRs]
                               .sort(([w, h], [w2, h2]) => w2 - w)
                               .map(([width, height]) => ({
                                      audio: false,
                                      video: {
                                        deviceId: {exact: deviceId},
                                        frameRate: {min: 24},
                                        width,
                                        height,
                                      },
                                    }));
        // Format of map result:
        // [
        //   [[CaptureW 1, CaptureH 1], [CaptureW 2, CaptureH 2], ...],
        //   [PreviewConstraint 1, PreviewConstraint 2, ...]
        // ]
        return [captureR, candidates];
      })
      .sort(([[w, h]], [[w2, h2]]) => {
        if (w == w2 && h == h2) {
          return 0;
        }
        if (w == prefR.width && h == prefR.height) {
          return -1;
        }
        if (w2 == prefR.width && h2 == prefR.height) {
          return 1;
        }
        return w2 * h2 - w * h;
      });
};
