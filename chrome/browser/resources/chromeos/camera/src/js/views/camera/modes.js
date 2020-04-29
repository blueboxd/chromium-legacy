// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from '../../browser_proxy/browser_proxy.js';
import {assert, assertInstanceof} from '../../chrome_util.js';
import {
  CaptureCandidate,           // eslint-disable-line no-unused-vars
  ConstraintsPreferrer,       // eslint-disable-line no-unused-vars
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from '../../device/constraints_preferrer.js';
import {Filenamer} from '../../models/filenamer.js';
import * as filesystem from '../../models/filesystem.js';
// eslint-disable-next-line no-unused-vars
import {VideoSaver} from '../../models/video_saver.js';
import {DeviceOperator, parseMetadata} from '../../mojo/device_operator.js';
import {CrosImageCapture} from '../../mojo/image_capture.js';
import {PerfEvent} from '../../perf.js';
import * as sound from '../../sound.js';
import * as state from '../../state.js';
import * as toast from '../../toast.js';
import {
  Facing,
  Mode,
  Resolution,
  ResolutionList,  // eslint-disable-line no-unused-vars
} from '../../type.js';
import * as util from '../../util.js';
import {RecordTime} from './recordtime.js';

/**
 * Contains video recording result.
 * @typedef {{
 *     resolution: {width: number, height: number},
 *     duration: number,
 *     videoSaver: !VideoSaver,
 * }}
 */
export let VideoResult;

/**
 * Contains photo taking result.
 * @typedef {{
 *     resolution: {width: number, height: number},
 *     blob: !Blob,
 * }}
 */
export let PhotoResult;

/**
 * Callback to trigger mode switching.
 * return {!Promise}
 * @typedef {function(): !Promise}
 */
export let DoSwitchMode;

/**
 * Callback for saving photo capture result.
 * param {!PhotoResult} Captured photo result.
 * param {string} Name of the photo result to be saved as.
 * return {!Promise}
 * @typedef {function(PhotoResult, string): !Promise}
 */
export let DoSavePhoto;

/**
 * Callback for allocating VideoSaver to save video capture result.
 * @typedef {function(): !Promise<!VideoSaver>}
 */
export let CreateVideoSaver;

/**
 * Callback for saving video capture result.
 * param {!VideoResult} Captured video result.
 * param {string} Name of the video result to be saved as.
 * return {!Promise}
 * @typedef {function(!VideoResult, string): !Promise}
 */
export let DoSaveVideo;

/**
 * Callback for playing shutter effect.
 * @typedef {function()}
 */
export let PlayShutterEffect;

/* eslint-disable no-unused-vars */

/**
 * The abstract interface for the mode configuration.
 * @interface
 */
class ModeConfig {
  /**
   * Factory function to create capture object for this mode.
   * @return {!ModeBase}
   * @abstract
   */
  captureFactory() {}

  /**
   * @param {?string} deviceId
   * @return {!Promise<boolean>} Resolves to boolean indicating whether the mode
   *     is supported by video device with specified device id.
   * @abstract
   */
  async isSupported(deviceId) {}

  /**
   * Get stream constraints for HALv1 of this mode.
   * @param {?string} deviceId
   * @return {!Promise<!Array<!MediaStreamConstraints>>}
   * @abstract
   */
  async getV1Constraints(deviceId) {}

  /* eslint-disable getter-return */

  /**
   * HALv3 constraints preferrer for this mode.
   * @return {!ConstraintsPreferrer}
   * @abstract
   */
  get constraintsPreferrer() {}

  /**
   * Mode to be fallbacked to when fail to configure this mode.
   * @return {!Mode}
   * @abstract
   */
  get nextMode() {}

  /**
   * Capture intent of this mode.
   * @return {!cros.mojom.CaptureIntent}
   * @abstract
   */
  get captureIntent() {}

  /* eslint-enable getter-return */
}

/* eslint-enable no-unused-vars */

/**
 * Mode controller managing capture sequence of different camera mode.
 */
export class Modes {
  /**
   * @param {!Mode} defaultMode Default mode to be switched to.
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   * @param {!DoSwitchMode} doSwitchMode
   * @param {!DoSavePhoto} doSavePhoto
   * @param {!CreateVideoSaver} createVideoSaver
   * @param {!DoSaveVideo} doSaveVideo
   * @param {!PlayShutterEffect} playShutterEffect
   */
  constructor(
      defaultMode, photoPreferrer, videoPreferrer, doSwitchMode, doSavePhoto,
      createVideoSaver, doSaveVideo, playShutterEffect) {
    /**
     * @type {!DoSwitchMode}
     * @private
     */
    this.doSwitchMode_ = doSwitchMode;

    /**
     * Capture controller of current camera mode.
     * @type {?ModeBase}
     */
    this.current = null;

    /**
     * Stream of current mode.
     * @type {?MediaStream}
     * @private
     */
    this.stream_ = null;

    /**
     * Camera facing of current mode.
     * @type {!Facing}
     * @private
     */
    this.facing_ = Facing.UNKNOWN;

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.modesGroup_ =
        assertInstanceof(document.querySelector('#modes-group'), HTMLElement);

    /**
     * @type {?Resolution}
     * @private
     */
    this.captureResolution_ = null;

    /**
     * Returns a set of available constraints for HALv1 device.
     * @param {boolean} videoMode Is getting constraints for video mode.
     * @param {?string} deviceId Id of video device.
     * @return {!Promise<!Array<!MediaStreamConstraints>>} Result of
     *     constraints-candidates.
     */
    const getV1Constraints = async function(videoMode, deviceId) {
      const defaultFacing = await util.getDefaultFacing();
      return [
        {
          aspectRatio: {ideal: videoMode ? 1.7777777778 : 1.3333333333},
          width: {min: 1280},
          frameRate: {min: 20, ideal: 30},
        },
        {
          width: {min: 640},
          frameRate: {min: 20, ideal: 30},
        },
      ].map((/** !MediaTrackConstraints */ constraint) => {
        if (deviceId) {
          constraint.deviceId = {exact: deviceId};
        } else {
          // HALv1 devices are unable to know facing before stream
          // configuration, deviceId is set to null for requesting camera with
          // default facing.
          constraint.facingMode = {exact: defaultFacing};
        }
        return {
          audio: videoMode ? {echoCancellation: false} : false,
          video: constraint,
        };
      });
    };

    /**
     * Mode classname and related functions and attributes.
     * @type {!Object<!Mode, !ModeConfig>}
     * @private
     */
    this.allModes_ = {
      [Mode.VIDEO]: {
        captureFactory: () => new Video(
            assertInstanceof(this.stream_, MediaStream), this.facing_,
            createVideoSaver, doSaveVideo),
        isSupported: async () => true,
        constraintsPreferrer: videoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, true),
        nextMode: Mode.PHOTO,
        captureIntent: cros.mojom.CaptureIntent.VIDEO_RECORD,
      },
      [Mode.PHOTO]: {
        captureFactory: () => new Photo(
            assertInstanceof(this.stream_, MediaStream), this.facing_,
            doSavePhoto, this.captureResolution_, playShutterEffect),
        isSupported: async () => true,
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: Mode.SQUARE,
        captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
      },
      [Mode.SQUARE]: {
        captureFactory: () => new Square(
            assertInstanceof(this.stream_, MediaStream), this.facing_,
            doSavePhoto, this.captureResolution_, playShutterEffect),
        isSupported: async () => true,
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: Mode.PHOTO,
        captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
      },
      [Mode.PORTRAIT]: {
        captureFactory: () => new Portrait(
            assertInstanceof(this.stream_, MediaStream), this.facing_,
            doSavePhoto, this.captureResolution_, playShutterEffect),
        isSupported: async (deviceId) => {
          if (deviceId === null) {
            return false;
          }
          const deviceOperator = await DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return false;
          }
          return await deviceOperator.isPortraitModeSupported(deviceId);
        },
        constraintsPreferrer: photoPreferrer,
        getV1Constraints: getV1Constraints.bind(this, false),
        nextMode: Mode.PHOTO,
        captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
      },
    };

    document.querySelectorAll('.mode-item>input').forEach((element) => {
      element.addEventListener('click', (event) => {
        if (!state.get(state.State.STREAMING) ||
            state.get(state.State.TAKING)) {
          event.preventDefault();
        }
      });
      element.addEventListener('change', async (event) => {
        if (element.checked) {
          const mode = element.dataset.mode;
          this.updateModeUI_(mode);
          state.set(state.State.MODE_SWITCHING, true);
          const isSuccess = await this.doSwitchMode_();
          state.set(state.State.MODE_SWITCHING, false, {hasError: !isSuccess});
        }
      });
    });

    [state.State.EXPERT, state.State.SAVE_METADATA].forEach(
        (/** state.State */ s) => {
          state.addObserver(s, this.updateSaveMetadata_.bind(this));
        });

    // Set default mode when app started.
    this.updateModeUI_(defaultMode);
  }

  /**
   * @return {!Array<Mode>}
   * @private
   */
  get allModeNames_() {
    return Object.keys(this.allModes_);
  }

  /**
   * Updates state of mode related UI to the target mode.
   * @param {!Mode} mode Mode to be toggled.
   * @private
   */
  updateModeUI_(mode) {
    this.allModeNames_.forEach((m) => state.set(m, m === mode));
    const element =
        document.querySelector(`.mode-item>input[data-mode=${mode}]`);
    element.checked = true;
    const wrapper = element.parentElement;
    let scrollTop = wrapper.offsetTop - this.modesGroup_.offsetHeight / 2 +
        wrapper.offsetHeight / 2;
    // Make photo mode scroll slightly upper so that the third mode item falls
    // in blur area: crbug.com/988869
    if (mode === Mode.PHOTO) {
      scrollTop -= 16;
    }
    this.modesGroup_.scrollTo({
      left: 0,
      top: scrollTop,
      behavior: 'smooth',
    });
  }

  /**
   * Gets all mode candidates. Desired trying sequence of candidate modes is
   * reflected in the order of the returned array.
   * @return {!Array<!Mode>} Mode candidates to be tried out.
   */
  getModeCandidates() {
    const tried = {};
    const /** !Array<Mode> */ results = [];
    let mode = this.allModeNames_.find(state.get);
    assert(mode !== undefined);
    while (!tried[mode]) {
      tried[mode] = true;
      results.push(mode);
      mode = this.allModes_[mode].nextMode;
    }
    return results;
  }

  /**
   * Gets all available capture resolution and its corresponding preview
   * constraints for the given mode.
   * @param {!Mode} mode
   * @param {string} deviceId
   * @param {!ResolutionList} previewResolutions
   * @return {!Array<!CaptureCandidate>}
   */
  getResolutionCandidates(mode, deviceId, previewResolutions) {
    return this.allModes_[mode].constraintsPreferrer.getSortedCandidates(
        deviceId, previewResolutions);
  }

  /**
   * Gets capture resolution and its corresponding preview constraints for the
   * given mode on camera HALv1 device.
   * @param {!Mode} mode
   * @param {?string} deviceId
   * @return {!Promise<!Array<!CaptureCandidate>>}
   */
  async getResolutionCandidatesV1(mode, deviceId) {
    const previewCandidates =
        await this.allModes_[mode].getV1Constraints(deviceId);
    return [{resolution: null, previewCandidates}];
  }

  /**
   * Gets capture intent for the given mode.
   * @param {!Mode} mode
   * @return {cros.mojom.CaptureIntent} Capture intent for the given mode.
   */
  getCaptureIntent(mode) {
    return this.allModes_[mode].captureIntent;
  }

  /**
   * Gets supported modes for video device of given device id.
   * @param {?string} deviceId Device id of the video device.
   * @return {!Promise<!Array<!Mode>>} All supported mode for
   *     the video device.
   */
  async getSupportedModes(deviceId) {
    const /** !Array<Mode> */ supportedModes = [];
    for (const mode of this.allModeNames_) {
      const obj = this.allModes_[mode];
      if (await obj.isSupported(deviceId)) {
        supportedModes.push(mode);
      }
    }
    return supportedModes;
  }

  /**
   * Updates mode selection UI according to given device id.
   * @param {?string} deviceId
   * @return {!Promise}
   */
  async updateModeSelectionUI(deviceId) {
    const supportedModes = await this.getSupportedModes(deviceId);
    document.querySelectorAll('.mode-item').forEach((element) => {
      const radio = element.querySelector('input[type=radio]');
      element.classList.toggle(
          'hide', !supportedModes.includes(radio.dataset.mode));
    });
    this.modesGroup_.classList.toggle('scrollable', supportedModes.length > 3);
    this.modesGroup_.classList.remove('hide');
  }

  /**
   * Creates and updates new current mode object.
   * @param {!Mode} mode Classname of mode to be updated.
   * @param {!MediaStream} stream Stream of the new switching mode.
   * @param {!Facing} facing Camera facing of the current mode.
   * @param {?string} deviceId Device id of currently working video device.
   * @param {?Resolution} captureResolution Capturing resolution width and
   *     height.
   * @return {!Promise}
   */
  async updateMode(mode, stream, facing, deviceId, captureResolution) {
    if (this.current !== null) {
      await this.current.stopCapture();
    }
    this.updateModeUI_(mode);
    this.stream_ = stream;
    this.facing_ = facing;
    this.captureResolution_ = captureResolution;
    this.current = this.allModes_[mode].captureFactory();
    if (deviceId && this.captureResolution_) {
      this.allModes_[mode].constraintsPreferrer.updateValues(
          deviceId, stream, facing, this.captureResolution_);
    }
    await this.updateSaveMetadata_();
  }

  /**
   * Checks whether to save image metadata or not.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async updateSaveMetadata_() {
    if (state.get(state.State.EXPERT) && state.get(state.State.SAVE_METADATA)) {
      await this.enableSaveMetadata_();
    } else {
      await this.disableSaveMetadata_();
    }
  }

  /**
   * Enables save metadata of subsequent photos in the current mode.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async enableSaveMetadata_() {
    if (this.current !== null) {
      await this.current.addMetadataObserver();
    }
  }

  /**
   * Disables save metadata of subsequent photos in the current mode.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async disableSaveMetadata_() {
    if (this.current !== null) {
      await this.current.removeMetadataObserver();
    }
  }
}

/**
 * Base class for controlling capture sequence in different camera modes.
 * @abstract
 */
class ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {?Resolution} captureResolution Capturing resolution width and
   *     height.
   */
  constructor(stream, facing, captureResolution) {
    /**
     * Stream of current mode.
     * @type {!MediaStream}
     * @protected
     */
    this.stream_ = stream;

    /**
     * Camera facing of current mode.
     * @type {!Facing}
     * @protected
     */
    this.facing_ = facing;

    /**
     * Capture resolution. May be null on device not support of setting
     * resolution.
     * @type {?Resolution}
     * @private
     */
    this.captureResolution_ = captureResolution;

    /**
     * Promise for ongoing capture operation.
     * @type {?Promise}
     * @private
     */
    this.capture_ = null;
  }

  /**
   * Initiates video/photo capture operation.
   * @return {!Promise} Promise for ongoing capture operation.
   */
  startCapture() {
    if (this.capture_ === null) {
      this.capture_ = this.start_().finally(() => this.capture_ = null);
    }
    return this.capture_;
  }

  /**
   * Stops the ongoing capture operation.
   * @return {!Promise} Promise for ongoing capture operation.
   */
  async stopCapture() {
    this.stop_();
    return await this.capture_;
  }

  /**
   * Adds an observer to save image metadata.
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {}

  /**
   * Remove the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {}

  /**
   * Initiates video/photo capture operation under this mode.
   * @return {!Promise}
   * @protected
   * @abstract
   */
  async start_() {}

  /**
   * Stops the ongoing capture operation under this mode.
   * @protected
   */
  stop_() {}
}

/**
 * Video recording MIME type. Mkv with AVC1 is the only preferred format.
 * @type {string}
 */
const VIDEO_MIMETYPE = browserProxy.isMp4RecordingEnabled() ?
    'video/x-matroska;codecs=avc1,pcm' :
    'video/x-matroska;codecs=avc1';

/**
 * Video mode capture controller.
 */
class Video extends ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {!CreateVideoSaver} createVideoSaver
   * @param {!DoSaveVideo} doSaveVideo
   */
  constructor(stream, facing, createVideoSaver, doSaveVideo) {
    super(stream, facing, null);

    /**
     * @type {!CreateVideoSaver}
     * @private
     */
    this.createVideoSaver_ = createVideoSaver;

    /**
     * @type {!DoSaveVideo}
     * @private
     */
    this.doSaveVideo_ = doSaveVideo;

    /**
     * Promise for play start sound delay.
     * @type {?Promise}
     * @private
     */
    this.startSound_ = null;

    /**
     * MediaRecorder object to record motion pictures.
     * @type {?MediaRecorder}
     * @private
     */
    this.mediaRecorder_ = null;

    /**
     * Record-time for the elapsed recording time.
     * @type {!RecordTime}
     * @private
     */
    this.recordTime_ = new RecordTime();
  }

  /**
   * @override
   */
  async start_() {
    this.startSound_ = sound.play('#sound-rec-start');
    try {
      await this.startSound_;
    } finally {
      this.startSound_ = null;
    }

    if (this.mediaRecorder_ === null) {
      try {
        if (!MediaRecorder.isTypeSupported(VIDEO_MIMETYPE)) {
          throw new Error('The preferred mimeType is not supported.');
        }
        this.mediaRecorder_ =
            new MediaRecorder(this.stream_, {mimeType: VIDEO_MIMETYPE});
      } catch (e) {
        toast.show('error_msg_record_start_failed');
        throw e;
      }
    }

    this.recordTime_.start();
    let /** ?VideoSaver */ videoSaver = null;
    let /** number */ duration = 0;
    try {
      videoSaver = await this.captureVideo_();
    } catch (e) {
      toast.show('error_msg_empty_recording');
      throw e;
    } finally {
      duration = this.recordTime_.stop();
    }
    sound.play('#sound-rec-end');

    const settings = this.stream_.getVideoTracks()[0].getSettings();
    const resolution = new Resolution(settings.width, settings.height);
    state.set(PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, true);
    try {
      await this.doSaveVideo_(
          {resolution, duration, videoSaver}, (new Filenamer()).newVideoName());
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facing_});
    } catch (e) {
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }
  }

  /**
   * @override
   */
  stop_() {
    if (this.startSound_ && this.startSound_.cancel) {
      this.startSound_.cancel();
    }
    if (this.mediaRecorder_ && this.mediaRecorder_.state === 'recording') {
      this.mediaRecorder_.stop();
    }
  }

  /**
   * Starts recording and waits for stop recording event triggered by stop
   * shutter.
   * @return {!Promise<!VideoSaver>} Saves recorded video.
   * @private
   */
  async captureVideo_() {
    const saver = await this.createVideoSaver_();

    return new Promise((resolve, reject) => {
      let noChunk = true;

      const ondataavailable = (event) => {
        if (event.data && event.data.size > 0) {
          noChunk = false;
          saver.write(event.data);
        }
      };
      const onstop = (event) => {
        this.mediaRecorder_.removeEventListener(
            'dataavailable', ondataavailable);
        this.mediaRecorder_.removeEventListener('stop', onstop);

        if (noChunk) {
          reject(new Error('Video blob error.'));
        } else {
          // TODO(yuli): Handle insufficient storage.
          resolve(saver);
        }
      };
      this.mediaRecorder_.addEventListener('dataavailable', ondataavailable);
      this.mediaRecorder_.addEventListener('stop', onstop);
      this.mediaRecorder_.start(100);
    });
  }
}

/**
 * Photo mode capture controller.
 */
class Photo extends ModeBase {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {!DoSavePhoto} doSavePhoto
   * @param {?Resolution} captureResolution
   * @param {!PlayShutterEffect} playShutterEffect
   */
  constructor(
      stream, facing, doSavePhoto, captureResolution, playShutterEffect) {
    super(stream, facing, captureResolution);

    /**
     * Callback for saving picture.
     * @type {!DoSavePhoto}
     * @protected
     */
    this.doSavePhoto_ = doSavePhoto;

    /**
     * CrosImageCapture object to capture still photos.
     * @type {?CrosImageCapture}
     * @private
     */
    this.crosImageCapture_ = null;

    /**
     * The observer id for saving metadata.
     * @type {?number}
     * @private
     */
    this.metadataObserverId_ = null;

    /**
     * Metadata names ready to be saved.
     * @type {!Array<string>}
     * @private
     */
    this.metadataNames_ = [];

    /**
     * Callback for playing shutter effect.
     * @type {!PlayShutterEffect}
     * @protected
     */
    this.playShutterEffect_ = playShutterEffect;
  }

  /**
   * @override
   */
  async start_() {
    if (this.crosImageCapture_ === null) {
      this.crosImageCapture_ =
          new CrosImageCapture(this.stream_.getVideoTracks()[0]);
    }

    await this.takePhoto_();
  }

  /**
   * Takes and saves a photo.
   * @return {!Promise}
   * @private
   */
  async takePhoto_() {
    const imageName = (new Filenamer()).newImageName();
    if (this.metadataObserverId_ !== null) {
      this.metadataNames_.push(Filenamer.getMetadataName(imageName));
    }

    let photoSettings;
    if (this.captureResolution_) {
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: this.captureResolution_.width,
        imageHeight: this.captureResolution_.height,
      });
    } else {
      const caps = await this.crosImageCapture_.getPhotoCapabilities();
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      });
    }

    state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, true);
    try {
      const results = await this.crosImageCapture_.takePhoto(photoSettings);

      state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, false, {facing: this.facing_});
      this.playShutterEffect_();

      state.set(PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, true);
      const blob = await results[0];
      const image = await util.blobToImage(blob);
      const resolution = new Resolution(image.width, image.height);
      await this.doSavePhoto_({resolution, blob}, imageName);
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facing_});
    } catch (e) {
      state.set(PerfEvent.PHOTO_CAPTURE_SHUTTER, false, {hasError: true});
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      toast.show('error_msg_take_photo_failed');
      throw e;
    }
  }

  /**
   * Adds an observer to save metadata.
   * @return {!Promise} Promise for the operation.
   */
  async addMetadataObserver() {
    if (!this.stream_) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const cameraMetadataTagInverseLookup = {};
    Object.entries(cros.mojom.CameraMetadataTag).forEach(([key, value]) => {
      if (key === 'MIN_VALUE' || key === 'MAX_VALUE') {
        return;
      }
      cameraMetadataTagInverseLookup[value] = key;
    });

    const callback = (metadata) => {
      const parsedMetadata = {};
      for (const entry of metadata.entries) {
        const key = cameraMetadataTagInverseLookup[entry.tag];
        if (key === undefined) {
          // TODO(kaihsien): Add support for vendor tags.
          continue;
        }

        const val = parseMetadata(entry);
        parsedMetadata[key] = val;
      }

      filesystem.saveBlob(
          new Blob(
              [JSON.stringify(parsedMetadata, null, 2)],
              {type: 'application/json'}),
          this.metadataNames_.shift());
    };

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    this.metadataObserverId_ = await deviceOperator.addMetadataObserver(
        deviceId, callback, cros.mojom.StreamType.JPEG_OUTPUT);
  }

  /**
   * Removes the observer that saves metadata.
   * @return {!Promise} Promise for the operation.
   */
  async removeMetadataObserver() {
    if (!this.stream_ || this.metadataObserverId_ === null) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    const isSuccess = await deviceOperator.removeMetadataObserver(
        deviceId, this.metadataObserverId_);
    if (!isSuccess) {
      console.error(`Failed to remove metadata observer with id: ${
          this.metadataObserverId_}`);
    }
    this.metadataObserverId_ = null;
  }
}

/**
 * Square mode capture controller.
 */
class Square extends Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {!DoSavePhoto} doSavePhoto
   * @param {?Resolution} captureResolution
   * @param {!PlayShutterEffect} playShutterEffect
   */
  constructor(
      stream, facing, doSavePhoto, captureResolution, playShutterEffect) {
    super(stream, facing, doSavePhoto, captureResolution, playShutterEffect);

    this.doSavePhoto_ = async (result, ...args) => {
      // Since the image blob after square cut will lose its EXIF including
      // orientation information. Corrects the orientation before the square
      // cut.
      result.blob = await new Promise(
          (resolve, reject) => util.orientPhoto(result.blob, resolve, reject));
      result.blob = await this.cropSquare(result.blob);
      await doSavePhoto(result, ...args);
    };
  }

  /**
   * Crops out maximum possible centered square from the image blob.
   * @param {!Blob} blob
   * @return {!Promise<!Blob>} Promise with result cropped square image.
   */
  async cropSquare(blob) {
    const img = await util.blobToImage(blob);
    const side = Math.min(img.width, img.height);
    const canvas = document.createElement('canvas');
    canvas.width = side;
    canvas.height = side;
    const ctx = canvas.getContext('2d');
    ctx.drawImage(
        img, Math.floor((img.width - side) / 2),
        Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
    const croppedBlob = await new Promise((resolve) => {
      canvas.toBlob(resolve, 'image/jpeg');
    });
    croppedBlob.resolution = blob.resolution;
    return croppedBlob;
  }
}

/**
 * Portrait mode capture controller.
 */
class Portrait extends Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {!DoSavePhoto} doSavePhoto
   * @param {?Resolution} captureResolution
   * @param {!PlayShutterEffect} playShutterEffect
   */
  constructor(
      stream, facing, doSavePhoto, captureResolution, playShutterEffect) {
    super(stream, facing, doSavePhoto, captureResolution, playShutterEffect);
  }

  /**
   * @override
   */
  async start_() {
    if (this.crosImageCapture_ === null) {
      this.crosImageCapture_ =
          new CrosImageCapture(this.stream_.getVideoTracks()[0]);
    }

    let photoSettings;
    if (this.captureResolution_) {
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: this.captureResolution_.width,
        imageHeight: this.captureResolution_.height,
      });
    } else {
      const caps = await this.crosImageCapture_.getPhotoCapabilities();
      photoSettings = /** @type {!PhotoSettings} */ ({
        imageWidth: caps.imageWidth.max,
        imageHeight: caps.imageHeight.max,
      });
    }

    const filenamer = new Filenamer();
    const refImageName = filenamer.newBurstName(false);
    const portraitImageName = filenamer.newBurstName(true);

    if (this.metadataObserverId_ !== null) {
      [refImageName, portraitImageName].forEach((/** string */ imageName) => {
        this.metadataNames_.push(Filenamer.getMetadataName(imageName));
      });
    }

    let /** ?Promise<!Blob> */ reference;
    let /** ?Promise<!Blob> */ portrait;

    try {
      [reference, portrait] = await this.crosImageCapture_.takePhoto(
          photoSettings, [cros.mojom.Effect.PORTRAIT_MODE]);
      this.playShutterEffect_();
    } catch (e) {
      toast.show('error_msg_take_photo_failed');
      throw e;
    }

    state.set(PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, true);
    let hasError = false;

    /**
     * @param {!Promise<!Blob>} p
     * @param {string} imageName
     * @return {!Promise}
     */
    const saveResult = async (p, imageName) => {
      const isPortrait = Object.is(p, portrait);
      let /** ?Blob */ blob = null;
      try {
        blob = await p;
      } catch (e) {
        hasError = true;
        toast.show(
            isPortrait ? 'error_msg_take_portrait_photo_failed' :
                         'error_msg_take_photo_failed');
        throw e;
      }
      const {width, height} = await util.blobToImage(blob);
      await this.doSavePhoto_({resolution: {width, height}, blob}, imageName);
    };

    const refSave = saveResult(reference, refImageName);
    const portraitSave = saveResult(portrait, portraitImageName);
    try {
      await portraitSave;
    } catch (e) {
      hasError = true;
      // Portrait image may failed due to absence of human faces.
      // TODO(inker): Log non-intended error.
    }
    await refSave;
    state.set(
        PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, false,
        {hasError, facing: this.facing_});
  }
}
