// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../browser_proxy/browser_proxy.js';
import * as state from '../state.js';
import {
  Facing,
  Mode,
  Resolution,
  ResolutionList,  // eslint-disable-line no-unused-vars
} from '../type.js';
// eslint-disable-next-line no-unused-vars
import {Camera3DeviceInfo} from './camera3_device_info.js';

/**
 * Candidate of capturing with specified photo or video resolution and
 * constraints-candidates it corresponding preview.
 * Video/photo capture resolution and the constraints-candidates of its
 * corresponding preview stream.
 * @typedef {{
 *   resolution: !Resolution,
 *   previewCandidates: !Array<!MediaStreamConstraints>
 * }}
 */
export let CaptureCandidate;

/**
 * Controller for managing preference of capture settings and generating a list
 * of stream constraints-candidates sorted by user preference.
 * @abstract
 */
export class ConstraintsPreferrer {
  /**
   * @param {!function()} doReconfigureStream Trigger stream reconfiguration to
   *     reflect changes in user preferred settings.
   * @protected
   */
  constructor(doReconfigureStream) {
    /**
     * @type {!function()}
     * @protected
     */
    this.doReconfigureStream_ = doReconfigureStream;

    /**
     * Object saving resolution preference that each of its key as device id and
     * value to be preferred width, height of resolution of that video device.
     * @type {!Object<string, !Resolution>}
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
     * @type {!Object<string, !ResolutionList>}
     * @protected
     */
    this.deviceResolutions_ = {};

    /**
     * Listener for changes of preferred resolution used on particular video
     * device.
     * @type {!function(string, !Resolution)}
     * @private
     */
    this.preferredResolutionChangeListener_ = () => {};
  }

  /**
   * Restores saved preferred capture resolution per video device.
   * @param {string} key Key of local storage saving preferences.
   * @protected
   */
  restoreResolutionPreference_(key) {
    // TODO(inker): Return promise and await it to assure preferences are loaded
    // before any access.
    browserProxy.localStorageGet({[key]: {}}).then((values) => {
      this.prefResolution_ = {};
      for (const [deviceId, {width, height}] of Object.entries(values[key])) {
        this.prefResolution_[deviceId] = new Resolution(width, height);
      }
    });
  }

  /**
   * Saves preferred capture resolution per video device.
   * @param {string} key Key of local storage saving preferences.
   * @protected
   */
  saveResolutionPreference_(key) {
    browserProxy.localStorageSet({[key]: this.prefResolution_});
  }

  /**
   * Gets user preferred capture resolution for a specific device.
   * @param {string} deviceId Device id of the device.
   * @return {?Resolution} Returns preferred resolution or null if no preferred
   *     resolution found in user preference.
   */
  getPrefResolution(deviceId) {
    return this.prefResolution_[deviceId] || null;
  }

  /**
   * Updates with new video device information.
   * @param {!Array<!Camera3DeviceInfo>} devices
   * @abstract
   */
  updateDevicesInfo(devices) {}

  /**
   * Updates values according to currently working video device and capture
   * settings.
   * @param {string} deviceId Device id of video device to be updated.
   * @param {!MediaStream} stream Currently active preview stream.
   * @param {!Facing} facing Camera facing of video device to be updated.
   * @param {!Resolution} resolution Resolution to be updated to.
   * @abstract
   */
  updateValues(deviceId, stream, facing, resolution) {}

  /**
   * Gets all available candidates for capturing under this controller and its
   * corresponding preview constraints for the specified video device. Returned
   * resolutions and constraints candidates are both sorted in desired trying
   * order.
   * @abstract
   * @param {string} deviceId Device id of video device.
   * @param {!ResolutionList} previewResolutions Available preview resolutions
   *     for the video device.
   * @return {!Array<!CaptureCandidate>} Capture resolution and its preview
   *     constraints-candidates.
   */
  getSortedCandidates(deviceId, previewResolutions) {}

  /**
   * Changes user preferred capture resolution.
   * @abstract
   * @param {string} deviceId Device id of the video device to be changed.
   * @param {!Resolution} resolution Preferred capture resolution.
   */
  changePreferredResolution(deviceId, resolution) {}

  /**
   * Sets listener for changes of preferred resolution used in taking photo on
   * particular video device.
   * @param {!function(string, !Resolution)} listener
   */
  setPreferredResolutionChangeListener(listener) {
    this.preferredResolutionChangeListener_ = listener;
  }
}

/**
 * All supported constant fps options of video recording.
 * @type {!Array<number>}
 */
const SUPPORTED_CONSTANT_FPS = [30, 60];

/**
 * Controller for handling video resolution preference.
 */
export class VideoConstraintsPreferrer extends ConstraintsPreferrer {
  /**
   * @param {!function()} doReconfigureStream
   * @public
   */
  constructor(doReconfigureStream) {
    super(doReconfigureStream);

    /**
     * Object saving information of device supported constant fps. Each of its
     * key as device id and value as an object mapping from resolution to all
     * constant fps options supported by that resolution.
     * @type {!Object<string, !Object<!Resolution, !Array<number>>>}
     * @private
     */
    this.constFpsInfo_ = {};

    /**
     * Object saving fps preference that each of its key as device id and value
     * as an object mapping from resolution to preferred constant fps for that
     * resolution.
     * @type {!Object<string, !Object<!Resolution, number>>}
     * @private
     */
    this.prefFpses_ = {};

    /**
     * @type {!HTMLButtonElement}
     * @const
     * @private
     */
    this.toggleFps_ = /** @type {!HTMLButtonElement} */ (
        document.querySelector('#toggle-fps'));

    /**
     * Currently in used recording resolution.
     * @type {!Resolution}
     * @protected
     */
    this.resolution_ = new Resolution(0, -1);
    this.restoreResolutionPreference_('deviceVideoResolution');
    this.restoreFpsPreference_();

    this.toggleFps_.addEventListener('click', (event) => {
      if (!state.get(state.State.STREAMING) || state.get(state.State.TAKING)) {
        event.preventDefault();
      }
    });
    this.toggleFps_.addEventListener('change', (event) => {
      this.setPreferredConstFps_(
          /** @type {string} */ (this.deviceId_), this.resolution_,
          this.toggleFps_.checked ? 60 : 30);
      state.set(state.State.MODE_SWITCHING, true);
      let hasError = false;
      this.doReconfigureStream_()
          .catch((error) => {
            hasError = true;
            throw error;
          })
          .finally(
              () => state.set(state.State.MODE_SWITCHING, false, {hasError}));
    });
  }

  /**
   * Restores saved preferred fps per video resolution per video device.
   * @private
   */
  restoreFpsPreference_() {
    browserProxy.localStorageGet({deviceVideoFps: {}})
        .then((values) => this.prefFpses_ = values.deviceVideoFps);
  }

  /**
   * Saves preferred fps per video resolution per video device.
   * @private
   */
  saveFpsPreference_() {
    browserProxy.localStorageSet({deviceVideoFps: this.prefFpses_});
  }

  /**
   * @override
   */
  changePreferredResolution(deviceId, resolution) {
    this.prefResolution_[deviceId] = resolution;
    this.saveResolutionPreference_('deviceVideoResolution');
    if (state.get(Mode.VIDEO) && deviceId === this.deviceId_) {
      this.doReconfigureStream_();
    } else {
      this.preferredResolutionChangeListener_(deviceId, resolution);
    }
  }

  /**
   * Sets the preferred fps used in video recording for particular video device
   * with particular resolution.
   * @param {string} deviceId Device id of video device to be set with.
   * @param {!Resolution} resolution Resolution to be set with.
   * @param {number} prefFps Preferred fps to be set with.
   * @private
   */
  setPreferredConstFps_(deviceId, resolution, prefFps) {
    if (!SUPPORTED_CONSTANT_FPS.includes(prefFps)) {
      return;
    }
    this.toggleFps_.checked = prefFps === 60;
    SUPPORTED_CONSTANT_FPS.forEach(
        (fps) => state.set(state.assertState(`_${fps}fps`), fps === prefFps));
    this.prefFpses_[deviceId] = this.prefFpses_[deviceId] || {};
    this.prefFpses_[deviceId][resolution] = prefFps;
    this.saveFpsPreference_();
  }

  /**
   * @override
   */
  updateDevicesInfo(devices) {
    this.deviceResolutions_ = {};
    this.constFpsInfo_ = {};

    devices.forEach(({deviceId, videoResols, videoMaxFps, fpsRanges}) => {
      this.deviceResolutions_[deviceId] = videoResols;
      /**
       * @param {number} width
       * @param {number} height
       * @return {!Resolution|undefined}
       */
      const findResol = (width, height) =>
          videoResols.find((r) => r.width === width && r.height === height);
      /** @type {!Resolution} */
      let prefR = this.getPrefResolution(deviceId) || findResol(1920, 1080) ||
          findResol(1280, 720) || new Resolution(0, -1);
      if (findResol(prefR.width, prefR.height) === undefined) {
        prefR = videoResols.reduce(
            (maxR, R) => (maxR.area < R.area ? R : maxR),
            new Resolution(0, -1));
      }
      this.prefResolution_[deviceId] = prefR;

      const /** !Array<number> */ constFpses =
          fpsRanges.filter(({minFps, maxFps}) => minFps === maxFps)
              .map(({minFps}) => minFps);
      const /** !Object<(!Resolution|string), !Array<number>> */ fpsInfo = {};
      for (const [resolution, maxFps] of Object.entries(videoMaxFps)) {
        fpsInfo[/** @type {string} */ (resolution)] =
            constFpses.filter((fps) => fps <= maxFps);
      }
      this.constFpsInfo_[deviceId] = fpsInfo;
    });
    this.saveResolutionPreference_('deviceVideoResolution');
  }

  /**
   * @override
   */
  updateValues(deviceId, stream, facing, resolution) {
    this.deviceId_ = deviceId;
    this.resolution_ = resolution;
    this.prefResolution_[deviceId] = this.resolution_;
    this.saveResolutionPreference_('deviceVideoResolution');
    this.preferredResolutionChangeListener_(deviceId, this.resolution_);

    const fps = stream.getVideoTracks()[0].getSettings().frameRate;
    this.setPreferredConstFps_(deviceId, this.resolution_, fps);
    const supportedConstFpses =
        this.constFpsInfo_[deviceId][this.resolution_].filter(
            (fps) => SUPPORTED_CONSTANT_FPS.includes(fps));
    // Only enable multi fps UI on external camera.
    // See https://crbug.com/1059191 for details.
    state.set(
        state.State.MULTI_FPS,
        facing === Facing.EXTERNAL && supportedConstFpses.length > 1);
  }

  /**
   * @override
   */
  getSortedCandidates(deviceId, previewResolutions) {
    // Due to the limitation of MediaStream API, preview stream is used directly
    // to do video recording.

    /** @type {!Resolution} */
    const prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);
    /**
     * @param {!Resolution} r1
     * @param {!Resolution} r2
     * @return {number}
     */
    const sortPrefResol = (r1, r2) => {
      if (r1.equals(r2)) {
        return 0;
      }

      // Exactly the preferred resolution.
      if (r1.equals(prefR)) {
        return -1;
      }
      if (r2.equals(prefR)) {
        return 1;
      }

      // Aspect ratio same as preferred resolution.
      if (!r1.aspectRatioEquals(r2)) {
        if (r1.aspectRatioEquals(prefR)) {
          return -1;
        }
        if (r2.aspectRatioEquals(prefR)) {
          return 1;
        }
      }
      return r2.area - r1.area;
    };

    /**
     * Maps specified video resolution to object of resolution and all supported
     * constant fps under that resolution or null fps for not support constant
     * fps. The resolution-fpses are sorted by user preference of constant fps.
     * @param {!Resolution} r
     * @return {!Array<!{r: !Resolution, fps: number}>}
     */
    const getFpses = (r) => {
      let /** !Array<?number> */ constFpses = [null];
      /** @type {!Array<number>} */
      const constFpsInfo = this.constFpsInfo_[deviceId][r];
      // The higher constant fps will be ignored if constant 30 and 60 presented
      // due to currently lack of UI support for toggling it.
      if (constFpsInfo.includes(30) && constFpsInfo.includes(60)) {
        const prefFps =
            this.prefFpses_[deviceId] && this.prefFpses_[deviceId][r] || 30;
        constFpses = prefFps === 30 ? [30, 60] : [60, 30];
      } else {
        constFpses =
            [...constFpsInfo.filter((fps) => fps >= 30).sort().reverse(), null];
      }
      return constFpses.map((fps) => ({r, fps}));
    };

    /**
     * @param {!Resolution} r
     * @param {!number} fps
     * @return {!MediaStreamConstraints}
     */
    const toConstraints = ({width, height}, fps) => ({
      audio: {echoCancellation: false},
      video: {
        deviceId: {exact: deviceId},
        frameRate: fps ? {exact: fps} : {min: 20, ideal: 30},
        width,
        height,
      },
    });

    return [...this.deviceResolutions_[deviceId]]
        .sort(sortPrefResol)
        .flatMap(getFpses)
        .map(({r, fps}) => ({
               resolution: r,
               previewCandidates: [toConstraints(r, fps)],
             }));
  }
}

/**
 * Controller for handling photo resolution preference.
 */
export class PhotoConstraintsPreferrer extends ConstraintsPreferrer {
  /**
   * @param {!function()} doReconfigureStream
   * @public
   */
  constructor(doReconfigureStream) {
    super(doReconfigureStream);

    this.restoreResolutionPreference_('devicePhotoResolution');
  }

  /**
   * @override
   */
  changePreferredResolution(deviceId, resolution) {
    this.prefResolution_[deviceId] = resolution;
    this.saveResolutionPreference_('devicePhotoResolution');
    if (!state.get(Mode.VIDEO) && deviceId === this.deviceId_) {
      this.doReconfigureStream_();
    } else {
      this.preferredResolutionChangeListener_(deviceId, resolution);
    }
  }

  /**
   * @override
   */
  updateDevicesInfo(devices) {
    this.deviceResolutions_ = {};

    devices.forEach(({deviceId, photoResols}) => {
      this.deviceResolutions_[deviceId] = photoResols;
      /** @type {!Resolution} */
      let prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);
      if (!photoResols.some((r) => r.equals(prefR))) {
        prefR = photoResols.reduce(
            (maxR, R) => (maxR.area < R.area ? R : maxR),
            new Resolution(0, -1));
      }
      this.prefResolution_[deviceId] = prefR;
    });
    this.saveResolutionPreference_('devicePhotoResolution');
  }

  /**
   * @override
   */
  updateValues(deviceId, stream, facing, resolution) {
    this.deviceId_ = deviceId;
    this.prefResolution_[deviceId] = resolution;
    this.saveResolutionPreference_('devicePhotoResolution');
    this.preferredResolutionChangeListener_(deviceId, resolution);
  }

  /**
   * Finds and pairs photo resolutions and preview resolutions with the same
   * aspect ratio.
   * @param {!ResolutionList} captureResolutions Available photo capturing
   *     resolutions.
   * @param {!ResolutionList} previewResolutions Available preview resolutions.
   * @return {!Array<!{capture: !ResolutionList, preview: !ResolutionList}>}
   *     Each item of returned array is a object of capture and preview
   *     resolutions of same aspect ratio.
   * @private
   */
  pairCapturePreviewResolutions_(captureResolutions, previewResolutions) {
    const toSupportedPreviewRatio = (r) => {
      // Special aspect ratio mapping rule, see http://b/147986763.
      if (r.width === 848 && r.height === 480) {
        return (new Resolution(16, 9)).aspectRatio;
      }
      return r.aspectRatio;
    };
    /** @type {!Object<string, !ResolutionList>} */
    const previewRatios = previewResolutions.reduce((rs, r) => {
      const ratio = toSupportedPreviewRatio(r);
      rs[ratio] = rs[ratio] || [];
      rs[ratio].push(r);
      return rs;
    }, {});
    /** @type {!Object<string, !ResolutionList>} */
    const captureRatios = captureResolutions.reduce((rs, r) => {
      const ratio = toSupportedPreviewRatio(r);
      if (ratio in previewRatios) {
        rs[ratio] = rs[ratio] || [];
        rs[ratio].push(r);
      }
      return rs;
    }, {});
    return Object.entries(captureRatios)
        .map(([
               /** string */ aspectRatio,
               /** !Resolution */ capture,
             ]) => ({capture, preview: previewRatios[aspectRatio]}));
  }

  /**
   * @override
   */
  getSortedCandidates(deviceId, previewResolutions) {
    /** @type {!ResolutionList} */
    const photoResolutions = this.deviceResolutions_[deviceId];

    /** @type {!Resolution} */
    const prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);

    /**
     * @param {!CaptureCandidate} candidate
     * @param {!CaptureCandidate} candidate2
     * @return {number}
     */
    const sortPrefResol = ({resolution: r1}, {resolution: r2}) => {
      if (r1.equals(r2)) {
        return 0;
      }
      // Exactly the preferred resolution.
      if (r1.equals(prefR)) {
        return -1;
      }
      if (r2.equals(prefR)) {
        return 1;
      }
      return r2.area - r1.area;
    };

    /**
     * @param {!{capture: !ResolutionList, preview: !ResolutionList}} capture
     * @return {!CaptureCandidate}
     */
    const toCaptureCandidate = ({capture: captureRs, preview: previewRs}) => {
      let /** !Resolution */ captureR = prefR;
      if (!captureRs.some((r) => r.equals(prefR))) {
        captureR = captureRs.reduce(
            (captureR, r) => (r.width > captureR.width ? r : captureR));
      }

      /**
       * @param {!ResolutionList} resolutions
       * @return {!ResolutionList}
       */
      const sortPreview = (resolutions) => {
        if (resolutions.length === 0) {
          return [];
        }

        // Sorts the preview resolution (Rp) according to the capture resolution
        // (Rc) and the screen size (Rs) with the following orders:
        // If |Rc| <= |Rs|:
        //   1. All |Rp| <= |Rc|, and the larger, the better.
        //   2. All |Rp| > |Rc|, and the smaller, the better.
        //
        // If |Rc| > |Rs|:
        //   1. All |Rp| where |Rs| <= |Rp| <= |Rc|, and the smaller, the
        //   better.
        //   2. All |Rp| < |Rs|, and the larger, the better.
        //   3. All |Rp| > |Rc|, and the smaller, the better.
        //
        const Rs = Math.floor(window.screen.width * window.devicePixelRatio);
        const Rc = captureR.width;
        const cmpDescending = (r1, r2) => r2.width - r1.width;
        const cmpAscending = (r1, r2) => r1.width - r2.width;

        if (Rc <= Rs) {
          const notLargerThanRc =
              resolutions.filter((r) => r.width <= Rc).sort(cmpDescending);
          const largerThanRc =
              resolutions.filter((r) => r.width > Rc).sort(cmpAscending);
          return notLargerThanRc.concat(largerThanRc);
        } else {
          const betweenRsRc =
              resolutions.filter((r) => Rs <= r.width && r.width <= Rc)
                  .sort(cmpAscending);
          const smallerThanRs =
              resolutions.filter((r) => r.width < Rs).sort(cmpDescending);
          const largerThanRc =
              resolutions.filter((r) => r.width > Rc).sort(cmpAscending);
          return betweenRsRc.concat(smallerThanRs).concat(largerThanRc);
        }
      };

      const /** !Array<!MediaStreamConstraints> */ previewCandidates =
          sortPreview(previewRs).map(({width, height}) => ({
                                       audio: false,
                                       video: {
                                         deviceId: {exact: deviceId},
                                         width,
                                         height,
                                       },
                                     }));
      return {resolution: captureR, previewCandidates};
    };

    return this
        .pairCapturePreviewResolutions_(photoResolutions, previewResolutions)
        .map(toCaptureCandidate)
        .sort(sortPrefResol);
  }
}
