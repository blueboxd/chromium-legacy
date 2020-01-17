// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Used to save captured video.
 * @interface
 */
export class VideoSaver {
  /**
   * Writes video data to result video.
   * @param {!Blob} blob Video data to be written.
   * @return {!Promise}
   */
  async write(blob) {}

  /**
   * Finishes the write of video data parts and returns result video file.
   * @return {!Promise<!FileEntry>} Result video file.
   */
  async endWrite() {}
}
