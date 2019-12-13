// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * import {assert, assertInstanceof} from './chrome_util.js';
 */
var {assert, assertInstanceof} = {assert, assertInstanceof};

/**
 * Cover photo of gallery button.
 */
cca.CoverPhoto = class {
  /**
   * @param {!FileEntry} file File entry of cover photo.
   * @param {!string} thumbnailUrl Url to its thumbnail.
   */
  constructor(file, thumbnailUrl) {
    /**
     * @type {!FileEntry}
     * @const
     */
    this.file = file;

    /**
     * @type {string}
     * @const
     */
    this.thumbnailUrl = thumbnailUrl;
  }

  /**
   * File name of the cover photo.
   * @return {string}
   */
  get name() {
    return this.file.name;
  }

  /**
   * Releases resources used by this cover photo.
   */
  release() {
    URL.revokeObjectURL(this.thumbnailUrl);
  }

  /**
   * Creates CoverPhoto objects from photo file.
   * @param {!FileEntry} file
   * @return {!Promise<!cca.CoverPhoto>}
   */
  static async create(file) {
    const fileUrl = await cca.models.FileSystem.pictureURL(file);
    const isVideo = cca.models.FileSystem.hasVideoPrefix(file);
    const thumbnail = await cca.util.scalePicture(
        fileUrl, isVideo, cca.CoverPhoto.THUMBNAIL_WIDTH);
    URL.revokeObjectURL(fileUrl);

    return new cca.CoverPhoto(file, URL.createObjectURL(thumbnail));
  }
};

/**
 * Width of thumbnail.
 * @type {number}
 * @const
 */
cca.CoverPhoto.THUMBNAIL_WIDTH = 240;

/**
 * Creates a controller for the gallery-button.
 * @implements {cca.models.ResultSaver}
 */
cca.GalleryButton = class {
  /**
   * @public
   */
  constructor() {
    /**
     * Cover photo from latest saved picture.
     * @type {?cca.CoverPhoto}
     * @private
     */
    this.cover_ = null;

    /**
     * @type {!HTMLButtonElement}
     * @private
     */
    this.button_ = assertInstanceof(
        document.querySelector('#gallery-enter'), HTMLButtonElement);

    /**
     * Directory holding saved pictures showing in gallery.
     * @type {?DirectoryEntry}
     * @private
     */
    this.directory_ = null;

    this.button_.addEventListener('click', async (event) => {
      // Check if the last picture serving as cover photo still exists before
      // opening it in the gallery.
      // TODO(yuli): Remove this workaround for unable watching changed-files.
      await this.checkCover_();
      if (this.cover_ !== null) {
        this.openGallery_(this.cover_.file);
      }
    });
  }

  /**
   * Initializes the gallery button.
   * @param {!DirectoryEntry} dir Directory holding saved pictures showing in
   *     gallery.
   */
  async initialize(dir) {
    this.directory_ = dir;
    await this.checkCover_();
  }

  /**
   * @param {?FileEntry} file File to be set as cover photo.
   * @return {!Promise}
   * @private
   */
  async updateCover_(file) {
    const cover = file === null ? null : await cca.CoverPhoto.create(file);
    if (this.cover_ === cover) {
      return;
    }
    if (this.cover_ !== null) {
      this.cover_.release();
    }
    this.cover_ = cover;

    this.button_.hidden = cover === null;
    this.button_.style.backgroundImage =
        cover !== null ? `url("${cover.thumbnailUrl}")` : 'none';
  }

  /**
   * Checks validity of cover photo from download directory.
   * @private
   */
  async checkCover_() {
    if (this.directory_ === null) {
      return;
    }
    const dir = this.directory_;

    // Checks existence of cached cover photo.
    if (this.cover_ !== null) {
      const file = await cca.models.FileSystem.getFile(
          dir, this.cover_.name, /* create */ false);
      if (file !== null) {
        return;
      }
    }

    // Rescan file system.
    const files = await cca.models.FileSystem.getEntries();
    if (files.length === 0) {
      await this.updateCover_(null);
      return;
    }
    const filesWithTime = await Promise.all(files.map(
        async (file) => ({
          file,
          time:
              (await cca.models.FileSystem.getMetadata(file)).modificationTime,
        })));
    const lastFile =
        filesWithTime.reduce((last, cur) => last.time > cur.time ? last : cur)
            .file;
    await this.updateCover_(lastFile);
  }

  /**
   * Opens the gallery to browse the picture.
   * @param {!FileEntry} file File of picture to be browsed.
   * @private
   */
  openGallery_(file) {
    const id = 'nlkncpkkdoccmpiclbokaimcnedabhhm|app|open';
    chrome.fileManagerPrivate.executeTask(id, [file], (result) => {
      if (chrome.runtime.lastError) {
        console.warn(
            'Unable to open picture: ' + chrome.runtime.lastError.message);
        return;
      }
      if (result !== 'opened' && result !== 'message_sent') {
        console.warn('Unable to open picture: ' + result);
      }
    });
  }

  /**
   * @override
   */
  async savePhoto(blob, name) {
    const orientedPhoto = await new Promise((resolve) => {
      // Ignore errors since it is better to save something than
      // nothing.
      // TODO(yuli): Support showing images by EXIF orientation
      // instead.
      cca.util.orientPhoto(blob, resolve, () => resolve(blob));
    });
    const file = await cca.models.FileSystem.saveBlob(orientedPhoto, name);
    assert(file !== null);
    await this.updateCover_(file);
  }

  /**
   * @override
   */
  async startSaveVideo() {
    const tempFile = await cca.models.FileSystem.createTempVideoFile();
    return cca.models.FileVideoSaver.create(tempFile);
  }

  /**
   * @override
   */
  async finishSaveVideo(video, name) {
    const tempFile = await video.endWrite();
    const file = await cca.models.FileSystem.saveVideo(tempFile, name);
    assert(file !== null);
    await this.updateCover_(file);
  }
};
