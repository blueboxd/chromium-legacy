// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Throws from calls to methods requiring mojo supporting VCD on HALv1 device
 * equipped with legacy VCD implementation.
 */
export class LegacyVCDError extends Error {
  /**
   * @param {string=} message
   * @public
   */
  constructor(
      message =
          'Call to unsupported mojo operation on legacy VCD implementation.') {
    super(message);
    this.name = this.constructor.name;
  }
}
