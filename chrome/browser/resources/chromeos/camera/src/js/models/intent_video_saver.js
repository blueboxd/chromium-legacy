// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Intent} from '../intent.js';  // eslint-disable-line no-unused-vars
import {FileVideoSaver} from './file_video_saver.js';
import {createPrivateTempVideoFile} from './filesystem.js';
// eslint-disable-next-line no-unused-vars
import {VideoSaver} from './video_saver_interface.js';

/**
 * Used to save captured video into a preview file and forward to intent result.
 * @implements {VideoSaver}
 */
export class IntentVideoSaver {
  /**
   * @param {!Intent} intent
   * @param {!FileVideoSaver} fileSaver
   * @private
   */
  constructor(intent, fileSaver) {
    /**
     * @const {!Intent} intent
     * @private
     */
    this.intent_ = intent;

    /**
     * @const {!FileVideoSaver}
     * @private
     */
    this.fileSaver_ = fileSaver;
  }

  /**
   * @override
   */
  async write(blob) {
    await this.fileSaver_.write(blob);
    const arrayBuffer = await blob.arrayBuffer();
    this.intent_.appendData(new Uint8Array(arrayBuffer));
  }

  /**
   * @override
   */
  async endWrite() {
    return this.fileSaver_.endWrite();
  }

  /**
   * Creates IntentVideoSaver.
   * @param {!Intent} intent
   * @return {!Promise<!IntentVideoSaver>}
   */
  static async create(intent) {
    const tmpFile = await createPrivateTempVideoFile();
    const fileSaver = await FileVideoSaver.create(tmpFile);
    return new IntentVideoSaver(intent, fileSaver);
  }
}

/** @const */
cca.models.IntentVideoSaver = IntentVideoSaver;
