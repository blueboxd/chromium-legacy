// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
(function(global, binding, v8) {
  'use strict';

  const defineProperty = global.Object.defineProperty;

  class ByteLengthQueuingStrategy {
    constructor(options) {
      defineProperty(this, 'highWaterMark', {
        value: options.highWaterMark,
        enumerable: true,
        configurable: true,
        writable: true
      });
    }
    size(chunk) {
      return chunk.byteLength;
    }
  }

  binding.ByteLengthQueuingStrategy = ByteLengthQueuingStrategy;
});
