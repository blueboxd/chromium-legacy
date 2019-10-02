// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

export class TestThemeProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getColors',
      'startObserving',
    ]);

    this.colors_ = {};
  }

  getColors() {
    this.methodCalled('getColors');
    return Promise.resolve(this.colors_);
  }

  setColors(colors) {
    this.colors_ = colors;
  }

  startObserving() {
    this.methodCalled('startObserving');
  }
}
