// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

export class TestTabStripViewProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getColors',
      'isVisible',
      'observeThemeChanges',
    ]);

    this.visible_ = false;
  }

  getColors() {
    this.methodCalled('getColors');
    return Promise.resolve(this.colors_);
  }

  isVisible() {
    this.methodCalled('isVisible');
    return this.visible_;
  }

  setColors(colors) {
    this.colors_ = colors;
  }

  setVisible(visible) {
    this.visible_ = visible;
  }

  observeThemeChanges() {
    this.methodCalled('observeThemeChanges');
  }
}
