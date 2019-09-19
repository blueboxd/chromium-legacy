// Copyright (c) 2019 The Chromium Authors. All rights reserved.
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

/* eslint-disable no-unused-vars */

/**
 * Contains video recording result.
 * @typedef {{
 *     resolution: {width: number, height: number},
 *     duration: number,
 *     videoSaver: !cca.models.VideoSaver,
 * }}
 */
cca.views.camera.VideoResult;

/**
 * Contains photo taking result.
 * @typedef {{
 *     resolution: {width: number, height: number},
 *     blob: !Blob,
 * }}
 */
cca.views.camera.PhotoResult;

/* eslint-enable no-unused-vars */

/**
 * Callback to trigger mode switching.
 * @callback DoSwitchMode
 * @return {!Promise}
 */

/**
 * Callback for saving photo capture result.
 * @callback DoSavePhoto
 * @param {!cca.views.camera.PhotoResult} Captured photo result.
 * @param {string} Name of the photo result to be saved as.
 * @return {!Promise}
 */

/**
 * Callback for allocating VideoSaver to save video capture result.
 * @callback CreateVideoSaver
 * @return {!Promise<!cca.models.VideoSaver>}
 */

/**
 * Callback for saving video capture result.
 * @callback DoSaveVideo
 * @param {!cca.views.camera.VideoResult} Captured video result.
 * @param {string} Name of the video result to be saved as.
 * @return {!Promise}
 */

/**
 * Mode controller managing capture sequence of different camera mode.
 * @param {cca.device.PhotoResolPreferrer} photoResolPreferrer
 * @param {cca.device.VideoConstraintsPreferrer} videoPreferrer
 * @param {!DoSwitchMode} doSwitchMode
 * @param {!DoSavePhoto} doSavePhoto
 * @param {!CreateVideoSaver} createVideoSaver
 * @param {!DoSaveVideo} doSaveVideo
 * @constructor
 */
cca.views.camera.Modes = function(
    photoResolPreferrer, videoPreferrer, doSwitchMode, doSavePhoto,
    createVideoSaver, doSaveVideo) {
  /**
   * @type {!DoSwitchMode}
   * @private
   */
  this.doSwitchMode_ = doSwitchMode;

  /**
   * Capture controller of current camera mode.
   * @type {cca.views.camera.Mode}
   */
  this.current = null;

  /**
   * Stream of current mode.
   * @type {MediaStream}
   * @private
   */
  this.stream_ = null;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.modesGroup_ = document.querySelector('#modes-group');

  /**
   * @type {?[number, number]}
   * @private
   */
  this.captureResolution_ = null;

  /**
   * Mode classname and related functions and attributes.
   * @type {Object<string, Object>}
   * @private
   */
  this.allModes_ = {
    'video-mode': {
      captureFactory: () => new cca.views.camera.Video(
          this.stream_, createVideoSaver, doSaveVideo),
      isSupported: async () => true,
      resolutionConfig: videoPreferrer,
      v1Config: cca.views.camera.Modes.getV1Constraints.bind(this, true),
      nextMode: 'photo-mode',
      captureIntent: cros.mojom.CaptureIntent.VIDEO_RECORD,
    },
    'photo-mode': {
      captureFactory: () => new cca.views.camera.Photo(
          this.stream_, doSavePhoto, this.captureResolution_),
      isSupported: async () => true,
      resolutionConfig: photoResolPreferrer,
      v1Config: cca.views.camera.Modes.getV1Constraints.bind(this, false),
      nextMode: 'square-mode',
      captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
    },
    'square-mode': {
      captureFactory: () => new cca.views.camera.Square(
          this.stream_, doSavePhoto, this.captureResolution_),
      isSupported: async () => true,
      resolutionConfig: photoResolPreferrer,
      v1Config: cca.views.camera.Modes.getV1Constraints.bind(this, false),
      nextMode: 'portrait-mode',
      captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
    },
    'portrait-mode': {
      captureFactory: () => new cca.views.camera.Portrait(
          this.stream_, doSavePhoto, this.captureResolution_),
      isSupported: async (stream) => {
        const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
        if (!deviceOperator) {
          return false;
        }
        const deviceId = stream.getVideoTracks()[0].getSettings().deviceId;
        return await deviceOperator.isPortraitModeSupported(deviceId);
      },
      resolutionConfig: photoResolPreferrer,
      v1Config: cca.views.camera.Modes.getV1Constraints.bind(this, false),
      nextMode: 'photo-mode',
      captureIntent: cros.mojom.CaptureIntent.STILL_CAPTURE,
    },
  };

  // End of properties, seal the object.
  Object.seal(this);
  document.querySelectorAll('.mode-item>input').forEach((element) => {
    element.addEventListener('click', (event) => {
      if (!cca.state.get('streaming') || cca.state.get('taking')) {
        event.preventDefault();
      }
    });
    element.addEventListener('change', (event) => {
      if (element.checked) {
        var mode = element.dataset.mode;
        this.updateModeUI_(mode);
        cca.state.set('mode-switching', true);
        this.doSwitchMode_().then(() => cca.state.set('mode-switching', false));
      }
    });
  });

  ['expert', 'save-metadata'].forEach((state) => {
    cca.state.addObserver(state, this.updateSaveMetadata_.bind(this));
  });

  // Set default mode when app started.
  this.updateModeUI_('photo-mode');
};

/**
 * Updates state of mode related UI to the target mode.
 * @param {string} mode Mode to be toggled.
 */
cca.views.camera.Modes.prototype.updateModeUI_ = function(mode) {
  Object.keys(this.allModes_).forEach((m) => cca.state.set(m, m == mode));
  const element = document.querySelector(`.mode-item>input[data-mode=${mode}]`);
  element.checked = true;
  const wrapper = element.parentElement;
  let scrollTop = wrapper.offsetTop - this.modesGroup_.offsetHeight / 2 +
      wrapper.offsetHeight / 2;
  // Make photo mode scroll slightly upper so that the third mode item falls in
  // blur area: crbug.com/988869
  if (mode === 'photo-mode') {
    scrollTop -= 16;
  }
  this.modesGroup_.scrollTo({
    left: 0,
    top: scrollTop,
    behavior: 'smooth',
  });
};

/**
 * Returns a set of available constraints for HALv1 device.
 * @param {boolean} videoMode Is getting constraints for video mode.
 * @param {?string} deviceId Id of video device.
 * @return {Array<Object>} Result of constraints-candidates.
 */
cca.views.camera.Modes.getV1Constraints = function(videoMode, deviceId) {
  return [
    {
      aspectRatio: {ideal: videoMode ? 1.7777777778 : 1.3333333333},
      width: {min: 1280},
      frameRate: {min: 24},
    },
    {
      width: {min: 640},
      frameRate: {min: 24},
    },
  ].map((constraint) => {
    if (deviceId) {
      constraint.deviceId = {exact: deviceId};
    } else {
      // HALv1 devices are unable to know facing before stream configuration,
      // deviceId is set to null for requesting user facing camera as default.
      constraint.facingMode = {exact: 'user'};
    }
    return {audio: videoMode, video: constraint};
  });
};

/**
 * Switches mode to either video-recording or photo-taking.
 * @param {string} mode Class name of the switching mode.
 * @private
 */
cca.views.camera.Modes.prototype.switchMode_ = function(mode) {
  if (!cca.state.get('streaming') || cca.state.get('taking')) {
    return;
  }
  this.updateModeUI_(mode);
  cca.state.set('mode-switching', true);
  this.doSwitchMode_().then(() => cca.state.set('mode-switching', false));
};

/**
 * Gets all mode candidates. Desired trying sequence of candidate modes is
 * reflected in the order of the returned array.
 * @return {Array<string>} Mode candidates to be tried out.
 */
cca.views.camera.Modes.prototype.getModeCandidates = function() {
  const tried = {};
  const results = [];
  let mode = Object.keys(this.allModes_).find(cca.state.get);
  while (!tried[mode]) {
    tried[mode] = true;
    results.push(mode);
    mode = this.allModes_[mode].nextMode;
  }
  return results;
};

/**
 * Gets all available capture resolution and its corresponding preview
 * constraints for the given mode.
 * @param {string} mode
 * @param {string} deviceId
 * @param {ResolList} previewResolutions
 * @return {Array<[[number, number], Array<Object>]>} Result capture resolution
 *     width, height and constraints-candidates for its preview.
 */
cca.views.camera.Modes.prototype.getResolutionCandidates = function(
    mode, deviceId, previewResolutions) {
  return this.allModes_[mode].resolutionConfig.getSortedCandidates(
      deviceId, previewResolutions);
};

/**
 * Gets capture resolution and its corresponding preview constraints for the
 * given mode on camera HALv1 device.
 * @param {string} mode
 * @param {?string} deviceId
 * @return {Array<[?[number, number], Array<Object>]>} Result capture resolution
 *     width, height and constraints-candidates for its preview.
 */
cca.views.camera.Modes.prototype.getResolutionCandidatesV1 = function(
    mode, deviceId) {
  return this.allModes_[mode].v1Config(deviceId).map(
      (constraints) => [null, [constraints]]);
};

/**
 * Gets capture intent for the given mode.
 * @param {string} mode
 * @return {cros.mojom.CaptureIntent} Capture intent for the given mode.
 */
cca.views.camera.Modes.prototype.getCaptureIntent = function(mode) {
  return this.allModes_[mode].captureIntent;
};

/**
 * Gets supported modes for video device of the given stream.
 * @param {MediaStream} stream Stream of the video device.
 * @return {Array<string>} Names of all supported mode for the video device.
 */
cca.views.camera.Modes.prototype.getSupportedModes = async function(stream) {
  let supportedModes = [];
  for (const [mode, obj] of Object.entries(this.allModes_)) {
    if (await obj.isSupported(stream)) {
      supportedModes.push(mode);
    }
  }
  return supportedModes;
};

/**
 * Updates mode selection UI according to given supported modes.
 * @param {Array<string>} supportedModes Supported mode names to be updated
 *     with.
 */
cca.views.camera.Modes.prototype.updateModeSelectionUI = function(
    supportedModes) {
  document.querySelectorAll('.mode-item').forEach((element) => {
    const radio = element.querySelector('input[type=radio]');
    element.classList.toggle(
        'hide', !supportedModes.includes(radio.dataset.mode));
  });
  this.modesGroup_.classList.toggle('scrollable', supportedModes.length > 3);
  this.modesGroup_.classList.remove('hide');
};

/**
 * Creates and updates new current mode object.
 * @async
 * @param {string} mode Classname of mode to be updated.
 * @param {MediaStream} stream Stream of the new switching mode.
 * @param {?string} deviceId Device id of currently working video device.
 * @param {?[number, number]} captureResolution Capturing resolution width and
 *     height.
 */
cca.views.camera.Modes.prototype.updateMode =
    async function(mode, stream, deviceId, captureResolution) {
  if (this.current != null) {
    await this.current.stopCapture();
  }
  this.updateModeUI_(mode);
  this.stream_ = stream;
  this.captureResolution_ = captureResolution;
  this.current = this.allModes_[mode].captureFactory();
  if (deviceId && this.captureResolution_) {
    this.allModes_[mode].resolutionConfig.updateValues(
        deviceId, stream, ...this.captureResolution_);
  }
  await this.updateSaveMetadata_();
};

/**
 * Checks whether to save image metadata or not.
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.camera.Modes.prototype.updateSaveMetadata_ = async function() {
  if (cca.state.get('expert') && cca.state.get('save-metadata')) {
    await this.enableSaveMetadata_();
  } else {
    await this.disableSaveMetadata_();
  }
};

/**
 * Enables save metadata of subsequent photos in the current mode.
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.camera.Modes.prototype.enableSaveMetadata_ = async function() {
  if (this.current !== null) {
    await this.current.addMetadataObserver();
  }
};

/**
 * Disables save metadata of subsequent photos in the current mode.
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.camera.Modes.prototype.disableSaveMetadata_ = async function() {
  if (this.current !== null) {
    await this.current.removeMetadataObserver();
  }
};

/**
 * Base class for controlling capture sequence in different camera modes.
 * @param {MediaStream} stream
 * @param {?[number, number]} captureResolution Capturing resolution width and
 *     height.
 * @constructor
 */
cca.views.camera.Mode = function(stream, captureResolution) {
  /**
   * Stream of current mode.
   * @type {?Promise}
   * @protected
   */
  this.stream_ = stream;

  /**
   * Width, height of capture resolution. May be null on device not supporting
   * setting resolution.
   * @type {?[number, number]}
   * @private
   */
  this.captureResolution_ = captureResolution;

  /**
   * Promise for ongoing capture operation.
   * @type {?Promise}
   * @private
   */
  this.capture_ = null;
};

/**
 * Initiates video/photo capture operation.
 * @return {?Promise} Promise for ongoing capture operation.
 */
cca.views.camera.Mode.prototype.startCapture = function() {
  if (!this.capture_) {
    this.capture_ = this.start_().finally(() => this.capture_ = null);
  }
  return this.capture_;
};

/**
 * Stops the ongoing capture operation.
 * @async
 * @return {Promise} Promise for ongoing capture operation.
 */
cca.views.camera.Mode.prototype.stopCapture = async function() {
  this.stop_();
  return await this.capture_;
};

/**
 * Adds an observer to save image metadata.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.Mode.prototype.addMetadataObserver = async function() {};

/**
 * Remove the observer that saves metadata.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.Mode.prototype.removeMetadataObserver = async function() {};

/**
 * Initiates video/photo capture operation under this mode.
 * @async
 * @protected
 */
cca.views.camera.Mode.prototype.start_ = async function() {};

/**
 * Stops the ongoing capture operation under this mode.
 * @protected
 */
cca.views.camera.Mode.prototype.stop_ = function() {};

/**
 * Video mode capture controller.
 * @param {MediaStream} stream
 * @param {!CreateVideoSaver} createVideoSaver
 * @param {!DoSaveVideo} doSaveVideo
 * @constructor
 */
cca.views.camera.Video = function(stream, createVideoSaver, doSaveVideo) {
  cca.views.camera.Mode.call(this, stream, null);

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
   * @type {MediaRecorder}
   * @private
   */
  this.mediaRecorder_ = null;

  /**
   * Record-time for the elapsed recording time.
   * @type {!cca.views.camera.RecordTime}
   * @private
   */
  this.recordTime_ = new cca.views.camera.RecordTime();

  // End of properties, seal the object.
  Object.seal(this);
};

cca.views.camera.Video.prototype = {
  __proto__: cca.views.camera.Mode.prototype,
};

/**
 * @override
 */
cca.views.camera.Video.prototype.start_ = async function() {
  this.startSound_ = cca.sound.play('#sound-rec-start');
  try {
    await this.startSound_;
  } finally {
    this.startSound_ = null;
  }

  if (this.mediaRecorder_ == null) {
    try {
      if (!MediaRecorder.isTypeSupported(
              cca.views.camera.Video.VIDEO_MIMETYPE)) {
        throw new Error('The preferred mimeType is not supported.');
      }
      this.mediaRecorder_ = new MediaRecorder(
          this.stream_, {mimeType: cca.views.camera.Video.VIDEO_MIMETYPE});
    } catch (e) {
      cca.toast.show('error_msg_record_start_failed');
      throw e;
    }
  }

  this.recordTime_.start();
  try {
    var videoSaver = await this.captureVideo_();
  } catch (e) {
    cca.toast.show('error_msg_empty_recording');
    throw e;
  } finally {
    var duration = this.recordTime_.stop();
  }
  cca.sound.play('#sound-rec-end');

  const {width, height} = this.stream_.getVideoTracks()[0].getSettings();
  await this.doSaveVideo_(
      {resolution: {width, height}, duration, videoSaver},
      (new cca.models.Filenamer()).newVideoName());
};

/**
 * @override
 */
cca.views.camera.Video.prototype.stop_ = function() {
  if (this.startSound_ && this.startSound_.cancel) {
    this.startSound_.cancel();
  }
  if (this.mediaRecorder_ && this.mediaRecorder_.state == 'recording') {
    this.mediaRecorder_.stop();
  }
};

/**
 * Video recording MIME type. Mkv with AVC1 is the only preferred
 * format.
 * @type {string}
 * @const
 */
cca.views.camera.Video.VIDEO_MIMETYPE = 'video/x-matroska;codecs=avc1';

/**
 * Starts recording and waits for stop recording event triggered by stop
 * shutter.
 * @return {!Promise<!cca.models.VideoSaver>} Saves recorded video.
 * @private
 */
cca.views.camera.Video.prototype.captureVideo_ = async function() {
  const saver = await this.createVideoSaver_();

  return new Promise((resolve, reject) => {
    let noChunk = true;

    var ondataavailable = (event) => {
      if (event.data && event.data.size > 0) {
        noChunk = false;
        saver.write(event.data);
      }
    };
    var onstop = (event) => {
      this.mediaRecorder_.removeEventListener('dataavailable', ondataavailable);
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
    this.mediaRecorder_.start(3000);
  });
};

/**
 * Photo mode capture controller.
 * @param {MediaStream} stream
 * @param {!DoSavePhoto} doSavePhoto
 * @param {?[number, number]} captureResolution
 * @constructor
 */
cca.views.camera.Photo = function(stream, doSavePhoto, captureResolution) {
  cca.views.camera.Mode.call(this, stream, captureResolution);

  /**
   * Callback for saving picture.
   * @type {!DoSavePhoto}
   * @protected
   */
  this.doSavePhoto_ = doSavePhoto;

  /**
   * ImageCapture object to capture still photos.
   * @type {?ImageCapture}
   * @private
   */
  this.imageCapture_ = null;

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
};

cca.views.camera.Photo.prototype = {
  __proto__: cca.views.camera.Mode.prototype,
};

/**
 * @override
 */
cca.views.camera.Photo.prototype.start_ = async function() {
  cca.sound.play('#sound-shutter');
  if (this.imageCapture_ == null) {
    try {
      this.imageCapture_ = new ImageCapture(this.stream_.getVideoTracks()[0]);
    } catch (e) {
      cca.toast.show('error_msg_take_photo_failed');
      throw e;
    }
  }

  const imageName = (new cca.models.Filenamer()).newImageName();
  if (this.metadataObserverId_ !== null) {
    this.metadataNames_.push(cca.models.Filenamer.getMetadataName(imageName));
  }

  try {
    var result = await this.createPhotoResult_();
  } catch (e) {
    cca.toast.show('error_msg_take_photo_failed');
    throw e;
  }

  await this.doSavePhoto_(result, imageName);
};

/**
 * Takes a photo and returns capture result.
 * @async
 * @return {!cca.views.camera.PhotoResult} Image capture result.
 * @private
 */
cca.views.camera.Photo.prototype.createPhotoResult_ = async function() {
  if (this.captureResolution_) {
    var photoSettings = {
      imageWidth: this.captureResolution_[0],
      imageHeight: this.captureResolution_[1],
    };
  } else {
    const caps = await this.imageCapture_.getPhotoCapabilities();
    photoSettings = {
      imageWidth: caps.imageWidth.max,
      imageHeight: caps.imageHeight.max,
    };
  }
  const blob = await this.imageCapture_.takePhoto(photoSettings);
  const {width, height} = await cca.util.blobToImage(blob);
  return {resolution: {width, height}, blob};
};

/**
 * Adds an observer to save metadata.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.Photo.prototype.addMetadataObserver = async function() {
  if (!this.stream_) {
    return;
  }

  const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
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

      const val = cca.mojo.parseMetadataData(entry);
      parsedMetadata[key] = val;
    }

    cca.models.FileSystem.saveBlob(
        new Blob(
            [JSON.stringify(parsedMetadata, null, 2)],
            {type: 'application/json'}),
        this.metadataNames_.shift());
  };

  const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
  this.metadataObserverId_ = await deviceOperator.addMetadataObserver(
      deviceId, callback, cros.mojom.StreamType.JPEG_OUTPUT);
};

/**
 * Removes the observer that saves metadata.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.Photo.prototype.removeMetadataObserver = async function() {
  if (!this.stream_ || this.metadataObserverId_ === null) {
    return;
  }

  const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
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
};

/**
 * Square mode capture controller.
 * @param {MediaStream} stream
 * @param {!DoSavePhoto} doSavePhoto
 * @param {?[number, number]} captureResolution
 * @constructor
 */
cca.views.camera.Square = function(stream, doSavePhoto, captureResolution) {
  cca.views.camera.Photo.call(this, stream, doSavePhoto, captureResolution);

  /**
   * Photo saving callback from parent.
   * @type {!DoSavePhoto}
   * @private
   */
  this.doAscentSave_ = this.doSavePhoto_;

  // End of properties, seal the object.
  Object.seal(this);

  this.doSavePhoto_ = async (result, ...args) => {
    result.blob = await this.cropSquare(result.blob);
    await this.doAscentSave_(result, ...args);
  };
};

cca.views.camera.Square.prototype = {
  __proto__: cca.views.camera.Photo.prototype,
};

/**
 * Crops out maximum possible centered square from the image blob.
 * @param {!Blob} blob
 * @return {!Blob} Promise with result cropped square image.
 * @async
 */
cca.views.camera.Square.prototype.cropSquare = async function(blob) {
  const img = await cca.util.blobToImage(blob);
  let side = Math.min(img.width, img.height);
  let canvas = document.createElement('canvas');
  canvas.width = side;
  canvas.height = side;
  let ctx = canvas.getContext('2d');
  ctx.drawImage(
      img, Math.floor((img.width - side) / 2),
      Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
  const croppedBlob = await new Promise((resolve) => {
    canvas.toBlob(resolve, 'image/jpeg');
  });
  croppedBlob.resolution = blob.resolution;
  return croppedBlob;
};

/**
 * Portrait mode capture controller.
 * @param {MediaStream} stream
 * @param {!DoSavePhoto} doSavePhoto
 * @param {?[number, number]} captureResolution
 * @constructor
 */
cca.views.camera.Portrait = function(stream, doSavePhoto, captureResolution) {
  cca.views.camera.Photo.call(this, stream, doSavePhoto, captureResolution);

  /**
   * ImageCapture object to capture still photos.
   * @type {?cca.mojo.ImageCapture}
   * @private
   */
  this.crosImageCapture_ = null;

  // End of properties, seal the object.
  Object.seal(this);
};

cca.views.camera.Portrait.prototype = {
  __proto__: cca.views.camera.Photo.prototype,
};

/**
 * @override
 */
cca.views.camera.Portrait.prototype.start_ = async function() {
  cca.sound.play('#sound-shutter');
  if (this.crosImageCapture_ == null) {
    try {
      this.crosImageCapture_ =
          new cca.mojo.ImageCapture(this.stream_.getVideoTracks()[0]);
    } catch (e) {
      cca.toast.show('error_msg_take_photo_failed');
      throw e;
    }
  }
  if (this.captureResolution_) {
    var photoSettings = {
      imageWidth: this.captureResolution_[0],
      imageHeight: this.captureResolution_[1],
    };
  } else {
    const caps = await this.imageCapture_.getPhotoCapabilities();
    photoSettings = {
      imageWidth: caps.imageWidth.max,
      imageHeight: caps.imageHeight.max,
    };
  }

  const filenamer = new cca.models.Filenamer();
  const refImageName = filenamer.newBurstName(false);
  const portraitImageName = filenamer.newBurstName(true);

  if (this.metadataObserverId_ !== null) {
    [refImageName, portraitImageName].forEach((imageName) => {
      this.metadataNames_.push(cca.models.Filenamer.getMetadataName(imageName));
    });
  }

  try {
    var [reference, portrait] = this.crosImageCapture_.takePhoto(
        photoSettings, [cros.mojom.Effect.PORTRAIT_MODE]);
  } catch (e) {
    cca.toast.show('error_msg_take_photo_failed');
    throw e;
  }

  const [refSave, portraitSave] = [
    [reference, refImageName],
    [portrait, portraitImageName],
  ].map(async ([p, imageName]) => {
    const isPortrait = Object.is(p, portrait);
    try {
      var blob = await p;
    } catch (e) {
      cca.toast.show(
          isPortrait ? 'error_msg_take_portrait_photo_failed' :
                       'error_msg_take_photo_failed');
      throw e;
    }
    const {width, height} = await cca.util.blobToImage(blob);
    await this.doSavePhoto_({resolution: {width, height}, blob}, imageName);
  });
  try {
    await portraitSave;
  } catch (e) {
    // Portrait image may failed due to absence of human faces.
    // TODO(inker): Log non-intended error.
  }
  await refSave;
};
