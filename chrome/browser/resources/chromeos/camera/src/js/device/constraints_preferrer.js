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

/**
 * Controller for managing preference of capture settings and generating a list
 * of stream constraints-candidates sorted by user preference.
 * @abstract
 */
cca.device.ConstraintsPreferrer = class {
  /**
   * @param {!cca.ResolutionEventBroker} resolBroker
   * @param {function()} doReconfigureStream Trigger stream reconfiguration to
   *     reflect changes in user preferred settings.
   * @protected
   */
  constructor(resolBroker, doReconfigureStream) {
    /**
     * @type {!cca.ResolutionEventBroker}
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
     * @type {!Object<string, {width: number, height: number}>}
     * @protected
     */
    this.prefResolution_ = {};

    /**
     * Device id of currently working video device.
     * @type {?string}
     * @protected
     */
    this.deviceId_ = null;

    /**
     * Object of device id as its key and all of available capture resolutions
     * supported by that video device as its value.
     * @type {!Object<string, !ResolList>}
     * @protected
     */
    this.deviceResolutions_ = {};
  }

  /**
   * Gets preferred capture resolution for a specific device.
   * @param {string} deviceId Device id of the device.
   * @return {!Array<number>} Preferred resolution formatted as [width, height].
   * @throws {Error}
   */
  getPrefResolution(deviceId) {
    const {width, height} = this.prefResolution_[deviceId] || {};
    if (width === undefined || height === undefined) {
      throw new Error(`Query non-existent device id ${deviceId}`);
    }
    return [width, height];
  }

  /**
   * Updates with new video device information.
   * @param {!Array<!cca.device.Camera3DeviceInfo>} devices
   * @abstract
   */
  updateDevicesInfo(devices) {}

  /**
   * Updates values according to currently working video device and capture
   * settings.
   * @param {string} deviceId Device id of video device to be updated.
   * @param {!MediaStream} stream Currently active preview stream.
   * @param {number} width Width of resolution to be updated to.
   * @param {number} height Height of resolution to be updated to.
   * @abstract
   */
  updateValues(deviceId, stream, width, height) {}

  /**
   * Gets all available candidates for capturing under this controller and its
   * corresponding preview constraints for the specified video device. Returned
   * resolutions and constraints candidates are both sorted in desired trying
   * order.
   * @abstract
   * @param {string} deviceId Device id of video device.
   * @param {!ResolList} previewResolutions Available preview resolutions for
   *     the video device.
   * @return {!Array<!Array<number>|!Array<!Object>>} Result capture
   *     resolution width, height and constraints-candidates for its preview.
   *     It's formatted as [[[w1, h1], [constraints_for_w1_h1...]],
   *     [[w2, h2], [constraints_for_w2_h2...]], ...].
   */
  getSortedCandidates(deviceId, previewResolutions) {}
};

/**
 * All supported constant fps options of video recording.
 * @type {!Array<number>}
 * @const
 */
cca.device.SUPPORTED_CONSTANT_FPS = [30, 60];

/**
 * Controller for handling video resolution preference.
 */
cca.device.VideoConstraintsPreferrer =
    class extends cca.device.ConstraintsPreferrer {
  /**
   * @param {!cca.ResolutionEventBroker} resolBroker
   * @param {function()} doReconfigureStream
   * @public
   */
  constructor(resolBroker, doReconfigureStream) {
    super(resolBroker, doReconfigureStream);

    /**
     * Object saving information of supported constant fps. Each of its key as
     * device id and value as an object mapping from resolution to all constant
     * fps options supported by that resolution.
     * @type {!Object<string, Object<Array<number>, Array<number>>>}
     * @private
     */
    this.constFpsInfo_ = {};

    /**
     * Object saving fps preference that each of its key as device id and value
     * as an object mapping from resolution to preferred constant fps for that
     * resolution.
     * @type {!Object<string, Object<Array<number>, number>>}
     * @private
     */
    this.prefFpses_ = {};

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.toggleFps_ = /** @type {!HTMLButtonElement} */ (
        document.querySelector('#toggle-fps'));

    /**
     * Currently in used recording resolution.
     * [width, height]
     * @type {!Array<number>}
     * @protected
     */
    this.resolution_ = [-1, -1];

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
            this.resolBroker_.notifyVideoPrefResolChange(
                deviceId, width, height);
          }
        });

    this.toggleFps_.addEventListener('click', (event) => {
      if (!cca.state.get('streaming') || cca.state.get('taking')) {
        event.preventDefault();
      }
    });
    this.toggleFps_.addEventListener('change', (event) => {
      this.setPreferredConstFps_(
          /** @type {string} */ (this.deviceId_), ...this.resolution_,
          this.toggleFps_.checked ? 60 : 30);
      this.doReconfigureStream_();
    });
  }

  /**
   * Sets the preferred fps used in video recording for particular video device
   * with particular resolution.
   * @param {string} deviceId Device id of video device to be set with.
   * @param {number} width Resolution width to be set with.
   * @param {number} height Resolution height to be set with.
   * @param {number} prefFps Preferred fps to be set with.
   * @private
   */
  setPreferredConstFps_(deviceId, width, height, prefFps) {
    if (!cca.device.SUPPORTED_CONSTANT_FPS.includes(prefFps)) {
      return;
    }
    this.toggleFps_.checked = prefFps === 60;
    cca.device.SUPPORTED_CONSTANT_FPS.forEach(
        (fps) => cca.state.set(`_${fps}fps`, fps == prefFps));
    this.prefFpses_[deviceId] = this.prefFpses_[deviceId] || {};
    this.prefFpses_[deviceId][[width, height]] = prefFps;
    cca.proxy.browserProxy.localStorageSet({deviceVideoFps: this.prefFpses_});
  }

  /**
   * @override
   */
  updateDevicesInfo(devices) {
    this.deviceResolutions_ = {};
    this.constFpsInfo_ = {};

    devices.forEach(({deviceId, videoResols, videoMaxFps, fpsRanges}) => {
      this.deviceResolutions_[deviceId] = videoResols;
      let {width = -1, height = -1} = this.prefResolution_[deviceId] || {};
      if (!videoResols.find(([w, h]) => w === width && h === height)) {
        [width, height] = videoResols.reduce(
            (maxR, R) => (maxR[0] * maxR[1] < R[0] * R[1] ? R : maxR), [0, 0]);
      }
      this.prefResolution_[deviceId] = {width, height};

      const constFpses = cca.device.SUPPORTED_CONSTANT_FPS.filter(
          (fps) => !!fpsRanges.find(
              ([minFps, maxFps]) => minFps === fps && maxFps === fps));
      const fpsInfo = {};
      for (const r of Object.keys(videoMaxFps).map((r) => r.toString())) {
        fpsInfo[r] = constFpses.filter((fps) => fps <= videoMaxFps[r]);
      }
      this.constFpsInfo_[deviceId] = fpsInfo;
    });
    cca.proxy.browserProxy.localStorageSet(
        {deviceVideoResolution: this.prefResolution_});
  }

  /**
   * @override
   */
  updateValues(deviceId, stream, width, height) {
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
  }

  /**
   * @override
   */
  getSortedCandidates(deviceId, previewResolutions) {
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

    // Maps specified video resolution width, height to tuple of width, height
    // and all supported constant fps under that resolution or null fps for not
    // support multiple constant fps options. The resolution-fps tuple are
    // sorted by user preference of constant fps.
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
  }
};


/**
 * Controller for handling photo resolution preference.
 */
cca.device.PhotoResolPreferrer = class extends cca.device.ConstraintsPreferrer {
  /**
   * @param {!cca.ResolutionEventBroker} resolBroker
   * @param {function()} doReconfigureStream
   * @public
   */
  constructor(resolBroker, doReconfigureStream) {
    super(resolBroker, doReconfigureStream);

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
            this.resolBroker_.notifyPhotoPrefResolChange(
                deviceId, width, height);
          }
        });
  }

  /**
   * @override
   */
  updateDevicesInfo(devices) {
    this.deviceResolutions_ = {};

    devices.forEach(({deviceId, photoResols}) => {
      this.deviceResolutions_[deviceId] = photoResols;
      let {width = -1, height = -1} = this.prefResolution_[deviceId] || {};
      if (!photoResols.find(([w, h]) => w == width && h == height)) {
        [width, height] = photoResols.reduce(
            (maxR, R) => (maxR[0] * maxR[1] < R[0] * R[1] ? R : maxR), [0, 0]);
      }
      this.prefResolution_[deviceId] = {width, height};
      return [deviceId, width, height, photoResols];
    });
    cca.proxy.browserProxy.localStorageSet(
        {devicePhotoResolution: this.prefResolution_});
  }

  /**
   * @override
   */
  updateValues(deviceId, stream, width, height) {
    this.deviceId_ = deviceId;
    this.prefResolution_[deviceId] = {width, height};
    cca.proxy.browserProxy.localStorageSet(
        {devicePhotoResolution: this.prefResolution_});
    this.resolBroker_.notifyPhotoPrefResolChange(deviceId, width, height);
  }

  /**
   * Finds and pairs photo resolutions and preview resolutions with the same
   * aspect ratio.
   * @param {!ResolList} captureResolutions Available photo capturing
   *     resolutions.
   * @param {!ResolList} previewResolutions Available preview resolutions.
   * @return {!Array<!Array<!ResolList>>} Each item of returned array is a pair
   *     of capture and preview resolutions of same aspect ratio.
   * @private
   */
  pairCapturePreviewResolutions_(captureResolutions, previewResolutions) {
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
  }

  /**
   * @override
   */
  getSortedCandidates(deviceId, previewResolutions) {
    const photoResolutions = this.deviceResolutions_[deviceId];
    const prefR = this.prefResolution_[deviceId] || {width: 0, height: -1};
    return this
        .pairCapturePreviewResolutions_(photoResolutions, previewResolutions)
        .map(([captureRs, previewRs]) => {
          if (captureRs.some(
                  ([w, h]) => w == prefR.width && h == prefR.height)) {
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
  }
};
