// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../chrome_util.js';

/**
 * Asynchronous job queue.
 */
export class AsyncJobQueue {
  /**
   * @public
   */
  constructor() {
    this.promise_ = Promise.resolve();
  }

  /**
   * Pushes the given job into queue.
   * @param {function(): !Promise} job
   */
  push(job) {
    this.promise_ = this.promise_.then(() => job());
  }

  /**
   * Flushes the job queue.
   * @return {!Promise} Resolved when all jobs in the queue are finished.
   */
  async flush() {
    await this.promise_;
  }
}

/**
 * Asynchronous writer.
 */
export class AsyncWriter {
  /**
   * @param {function(!Blob): !Promise} doWrite
   */
  constructor(doWrite) {
    /**
     * @type {!AsyncJobQueue}
     * @private
     */
    this.queue_ = new AsyncJobQueue();

    /**
     * @type {function(!Blob): !Promise}
     * @private
     */
    this.doWrite_ = doWrite;

    /**
     * @type {boolean}
     * @private
     */
    this.closed_ = false;
  }

  /**
   * Writes the blob asynchronously with |doWrite|.
   * @param {!Blob} blob
   */
  write(blob) {
    assert(!this.closed_);
    this.queue_.push(() => this.doWrite_(blob));
  }

  /**
   * Closes the writer. No more write operations are allowed.
   * @return {!Promise} Resolved when all write operations are finished.
   */
  async close() {
    this.closed_ = true;
    await this.queue_.flush();
  }

  /**
   * Combines multiple writers into one writer such that the blob would be
   * written to each of them.
   * @param {...!AsyncWriter} writers
   * @return {!AsyncWriter} The combined writer.
   */
  static combine(...writers) {
    const doWrite = (blob) => {
      return Promise.all(writers.map((writer) => writer.write(blob)));
    };
    return new AsyncWriter(doWrite);
  }
}
