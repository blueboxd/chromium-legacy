// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for models.
 */
cca.models = cca.models || {};

/**
 * Filenamer for single camera session.
 */
cca.models.Filenamer = class {
  /**
   * @param {number=} timestamp Timestamp of camera session.
   */
  constructor(timestamp) {
    /**
     * Timestamp of camera session.
     * @type {number}
     * @private
     */
    this.timestamp_ = timestamp === undefined ? Date.now() : timestamp;

    /**
     * Number of already saved burst images.
     * @type {number}
     * @private
     */
    this.burstCount_ = 0;
  }

  /**
   * Creates new filename for burst image.
   * @param {boolean} isCover If the image is set as cover of the burst.
   * @return {string} New filename.
   */
  newBurstName(isCover) {
    const prependZeros = (n, width) => {
      n = n + '';
      return new Array(Math.max(0, width - n.length) + 1).join('0') + n;
    };
    return cca.models.Filenamer.IMAGE_PREFIX +
        this.constructor.timestampToDatetimeName_(this.timestamp_) +
        cca.models.Filenamer.BURST_SUFFIX +
        prependZeros(++this.burstCount_, 5) +
        (isCover ? cca.models.Filenamer.BURST_COVER_SUFFIX : '') + '.jpg';
  }

  /**
   * Creates new filename for video.
   * @return {string} New filename.
   */
  newVideoName() {
    return cca.models.Filenamer.VIDEO_PREFIX +
        this.constructor.timestampToDatetimeName_(this.timestamp_) + '.mkv';
  }

  /**
   * Creates new filename for image.
   * @return {string} New filename.
   */
  newImageName() {
    return cca.models.Filenamer.IMAGE_PREFIX +
        this.constructor.timestampToDatetimeName_(this.timestamp_) + '.jpg';
  }

  /**
   * Transforms from capture timestamp to datetime name.
   * @param {number} timestamp Timestamp to be transformed.
   * @return {string} Transformed datetime name.
   * @private
   */
  static timestampToDatetimeName_(timestamp) {
    const pad = (n) => (n < 10 ? '0' : '') + n;
    var date = new Date(timestamp);
    return date.getFullYear() + pad(date.getMonth() + 1) + pad(date.getDate()) +
        '_' + pad(date.getHours()) + pad(date.getMinutes()) +
        pad(date.getSeconds());
  }

  /**
   * Get the metadata name from image name.
   * @param {string} imageName Name of image to derive the metadata name.
   * @return {string} Metadata name of the image.
   */
  static getMetadataName(imageName) {
    return imageName.replace(/\.[^/.]+$/, '.json');
  }
};

/**
 * The prefix of image files.
 * @type {string}
 * @const
 */
cca.models.Filenamer.IMAGE_PREFIX = 'IMG_';

/**
 * The prefix of video files.
 * @type {string}
 * @const
 */
cca.models.Filenamer.VIDEO_PREFIX = 'VID_';

/**
 * The suffix of burst image files.
 * @type {string}
 * @const
 */
cca.models.Filenamer.BURST_SUFFIX = '_BURST';

/**
 * The suffix of cover image for a series of burst image files.
 * @type {string}
 * @const
 */
cca.models.Filenamer.BURST_COVER_SUFFIX = '_COVER';
