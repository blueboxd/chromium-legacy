// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

export class TestTabStripEmbedderProxy extends TestBrowserProxy {
  constructor() {
    super([
      'closeContainer',
      'getColors',
      'getLayout',
      'isVisible',
      'observeThemeChanges',
      'showBackgroundContextMenu',
      'showTabContextMenu',
      'reportTabActivationDuration',
      'reportTabDataReceivedDuration',
      'reportTabCreationDuration',
    ]);

    this.colors_ = {};
    this.layout_ = {};
    this.visible_ = false;
  }

  getColors() {
    this.methodCalled('getColors');
    return Promise.resolve(this.colors_);
  }

  getLayout() {
    this.methodCalled('getLayout');
    return Promise.resolve(this.layout_);
  }

  isVisible() {
    this.methodCalled('isVisible');
    return this.visible_;
  }

  setColors(colors) {
    this.colors_ = colors;
  }

  setLayout(layout) {
    this.layout_ = layout;
  }

  setVisible(visible) {
    this.visible_ = visible;
  }

  observeThemeChanges() {
    this.methodCalled('observeThemeChanges');
  }

  closeContainer() {
    this.methodCalled('closeContainer');
    return Promise.resolve();
  }

  showBackgroundContextMenu(locationX, locationY) {
    this.methodCalled('showBackgroundContextMenu', [locationX, locationY]);
  }

  showTabContextMenu(tabId, locationX, locationY) {
    this.methodCalled('showTabContextMenu', [tabId, locationX, locationY]);
  }

  reportTabActivationDuration(durationMs) {
    this.methodCalled('reportTabActivationDuration', [durationMs]);
  }

  reportTabDataReceivedDuration(tabCount, durationMs) {
    this.methodCalled('reportTabDataReceivedDuration', [tabCount, durationMs]);
  }

  reportTabCreationDuration(tabCount, durationMs) {
    this.methodCalled('reportTabCreationDuration', [tabCount, durationMs]);
  }
}
