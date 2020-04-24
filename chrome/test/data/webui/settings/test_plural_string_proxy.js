// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PluralStringProxy. */

// clang-format off
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// clang-format on

/**
 * Test implementation
 * @implements {PluralStringProxy}
 * @constructor
 */
export class TestPluralStringProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getPluralString',
    ]);

    this.text = 'some text';
  }

  /** override */
  getPluralString(messageName, itemCount) {
    this.methodCalled('getPluralString', {messageName, itemCount});
    return Promise.resolve(this.text);
  }
}
