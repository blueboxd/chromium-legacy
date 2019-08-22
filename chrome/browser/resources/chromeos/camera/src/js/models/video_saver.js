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
 * Used to save captured video.
 */
cca.models.VideoSaver = class {
  /**
   * @param {!FileEntry} file
   * @param {!FileWriter} writer
   * @private
   */
  constructor(file, writer) {
    /**
     * @const {!FileEntry}
     */
    this.file_ = file;

    /**
     * @const {!FileWriter}
     */
    this.writer_ = writer;

    /**
     * Promise of the ongoing write.
     * @type {!Promise}
     */
    this.curWrite_ = Promise.resolve();
  }

  /**
   * Writes video data to result video.
   * @param {!Blob} blob Video data to be written.
   * @return {!Promise}
   */
  async write(blob) {
    this.curWrite_ = (async () => {
      await this.curWrite_;
      await new Promise((resolve) => {
        this.writer_.onwriteend = resolve;
        this.writer_.write(blob);
      });
    })();
    await this.curWrite_;
  }

  /**
   * Finishes the write of video data parts and returns result video file.
   * @return {!Promise<!FileEntry>} Result video file.
   */
  async endWrite() {
    await this.curWrite_;
    return this.file_;
  }

  /**
   * Create VideoSaver.
   * @param {!FileEntry} file The file which VideoSaver saves the result video
   *     into.
   * @return {!Promise<!cca.models.VideoSaver>}
   */
  static async create(file) {
    const writer = await new Promise(
        (resolve, reject) => file.createWriter(resolve, reject));
    return new cca.models.VideoSaver(file, writer);
  }
};
