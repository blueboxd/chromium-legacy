// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFileWriter} from './filesystem.js';
// eslint-disable-next-line no-unused-vars
import {VideoSaver} from './video_saver_interface.js';

/**
 * Used to save captured video.
 * @implements {VideoSaver}
 */
export class FileVideoSaver {
  /**
   * @param {!FileEntry} file
   * @param {!FileWriter} writer
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
   * @param {!Blob} blob
   * @protected
   */
  async doWrite_(blob) {
    return new Promise((resolve) => {
      this.writer_.onwriteend = resolve;
      this.writer_.write(blob);
    });
  }

  /**
   * @override
   */
  async write(blob) {
    this.curWrite_ = (async () => {
      await this.curWrite_;
      await this.doWrite_(blob);
    })();
    await this.curWrite_;
  }

  /**
   * @override
   */
  async endWrite() {
    await this.curWrite_;
    return this.file_;
  }

  /**
   * Creates FileVideoSaver.
   * @param {!FileEntry} file The file which FileVideoSaver saves the result
   *     video into.
   * @return {!Promise<!FileVideoSaver>}
   */
  static async createFileVideoSaver(file) {
    const writer = await getFileWriter(file);
    return new FileVideoSaver(file, writer);
  }
}
