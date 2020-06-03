// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 'AsyncBuffer' serves as a helper to buffer PerformanceObserver reports
// asynchronously.
class AsyncBuffer {
  constructor() {
    // 'pending' is an array that will buffer entries reported through the
    // PerformanceObserver and can be collected with 'pop'.
    this.pending = [];

    // 'resolve_fn' is a reference to the 'resolve' function of a
    // Promise that blocks for new entries to arrive via 'push()'. Calling
    // the function resolves the promise and unblocks calls to 'pop()'.
    this.resolve_fn = null;
  }

  // Concatenates the given 'entries' list to this AsyncBuffer.
  push(entries) {
    if (entries.length == 0) {
      throw new Error("Must not push an empty list of entries!");
    }
    this.pending = this.pending.concat(entries);

    // If there are calls to 'pop' that are blocked waiting for items, signal
    // that they can continue.
    if (this.resolve_fn != null) {
      this.resolve_fn();
      this.resolve_fn = null;
    }
  }

  // Takes the current pending entries from this AsyncBuffer. If there are no
  // entries queued already, this will block until some show up.
  async pop() {
    if (this.pending.length == 0) {
      // Need to instantiate a promise to block on. The next call to 'push'
      // will resolve the promise once it has queued the entries.
      await new Promise(resolve => {
        this.resolve_fn = resolve;
      });
    }
    assert_true(this.pending.length > 0);

    const result = this.pending;
    this.pending = [];
    return result;
  }
}