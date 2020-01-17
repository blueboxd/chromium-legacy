// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Intent} from '../intent.js';  // eslint-disable-line no-unused-vars
import {FileVideoSaver} from './file_video_saver.js';
import {createPrivateTempVideoFile, getFileWriter} from './filesystem.js';
// eslint-disable-next-line no-unused-vars
import {VideoSaver} from './video_saver_interface.js';

/**
 * Used to save captured video into a preview file and forward to intent result.
 */
export class IntentVideoSaver extends FileVideoSaver {
  /**
   * @param {!Intent} intent
   * @param {!FileEntry} file
   * @param {!FileWriter} writer
   */
  constructor(intent, file, writer) {
    super(file, writer);

    /**
     * @const {!Intent} intent
     * @private
     */
    this.intent_ = intent;
  }

  /**
   * @override
   */
  async doWrite_(blob) {
    await super.doWrite_(blob);
    const arrayBuffer = await blob.arrayBuffer();
    await this.intent_.appendData(new Uint8Array(arrayBuffer));
  }

  /**
   * Creates IntentVideoSaver.
   * @param {!Intent} intent
   * @return {!Promise<!IntentVideoSaver>}
   */
  static async createIntentVideoSaver(intent) {
    const tmpFile = await createPrivateTempVideoFile();
    const writer = await getFileWriter(tmpFile);
    return new IntentVideoSaver(intent, tmpFile, writer);
  }
}
