// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

export class TestTabsApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'activateTab',
      'closeTab',
      'getTabs',
      'moveTab',
      'trackThumbnailForTab',
    ]);

    this.tabs_;
  }

  activateTab(tabId) {
    this.methodCalled('activateTab', tabId);
    return Promise.resolve({active: true, id: tabId});
  }

  closeTab(tabId) {
    this.methodCalled('closeTab', tabId);
    return Promise.resolve();
  }

  getTabs() {
    this.methodCalled('getTabs');
    return Promise.resolve(this.tabs_.slice());
  }

  moveTab(tabId, newIndex) {
    this.methodCalled('moveTab', [tabId, newIndex]);
    return Promise.resolve();
  }

  setTabs(tabs) {
    this.tabs_ = tabs;
  }

  trackThumbnailForTab(tabId) {
    this.methodCalled('trackThumbnailForTab', tabId);
  }
}
