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
 * Creates the camera-intent-view controller.
 */
cca.views.CameraIntent = class extends cca.views.Camera {
  /**
   * @param {!cca.intent.Intent} intent
   * @param {!cca.device.DeviceInfoUpdater} infoUpdater
   * @param {!cca.device.PhotoResolPreferrer} photoPreferrer
   * @param {!cca.device.VideoConstraintsPreferrer} videoPreferrer
   */
  constructor(intent, infoUpdater, photoPreferrer, videoPreferrer) {
    const resultSaver = {
      savePhoto: async (blob) => {
        this.photoResult_ = blob;
        const buf = await blob.arrayBuffer();
        await this.intent_.appendData(new Uint8Array(buf));
      },
      startSaveVideo: async () => {
        return await cca.models.IntentVideoSaver.create(intent);
      },
      finishSaveVideo: async (video, savedName) => {
        this.videoResult_ = await video.endWrite();
      },
    };
    super(resultSaver, infoUpdater, photoPreferrer, videoPreferrer);

    /**
     * @type {!cca.intent.Intent}
     * @private
     */
    this.intent_ = intent;

    /**
     * @type {?Blob}
     * @private
     */
    this.photoResult_ = null;

    /**
     * @type {?FileEntry}
     * @private
     */
    this.videoResult_ = null;

    /**
     * @type {!cca.views.camera.ReviewResult}
     * @private
     */
    this.reviewResult_ = new cca.views.camera.ReviewResult();
  }

  /**
   * @override
   */
  beginTake_() {
    if (this.photoResult_ !== null) {
      URL.revokeObjectURL(this.photoResult_);
    }
    this.photoResult_ = null;
    this.videoResult_ = null;

    const take = super.beginTake_();
    if (take === null) {
      return null;
    }
    return (async () => {
      await take;

      if (this.photoResult_ === null && this.videoResult_ === null) {
        console.warn('End take without intent result.');
        return;
      }
      cca.state.set('suspend', true);
      await this.restart();
      const confirmed = await (
          this.photoResult_ !== null ?
              this.reviewResult_.openPhoto(this.photoResult_) :
              this.reviewResult_.openVideo(this.videoResult_));
      if (confirmed) {
        await this.intent_.finish();
        window.close();
        return;
      }
      cca.state.set('suspend', false);
      await this.intent_.clearData();
      await this.restart();
    })();
  }

  /**
   * @override
   */
  async startWithDevice_(deviceId) {
    return this.startWithMode_(deviceId, this.defaultMode);
  }
};
