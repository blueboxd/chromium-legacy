// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

class EventDispatcher {
  constructor() {
    this.eventListeners_ = [];
  }

  addListener(callback) {
    this.eventListeners_.push(callback);
  }

  dispatchEvent() {
    this.eventListeners_.forEach((callback) => {
      callback(...arguments);
    });
  }
}

export class TestTabsApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'activateTab',
      'closeTab',
      'getCurrentWindow',
      'moveTab',
      'trackThumbnailForTab',
    ]);

    this.callbackRouter = {
      onActivated: new EventDispatcher(),
      onCreated: new EventDispatcher(),
      onMoved: new EventDispatcher(),
      onRemoved: new EventDispatcher(),
      onUpdated: new EventDispatcher(),
    };

    this.currentWindow_;
  }

  activateTab(tabId) {
    this.methodCalled('activateTab', tabId);
    return Promise.resolve({active: true, id: tabId});
  }

  closeTab(tabId) {
    this.methodCalled('closeTab', tabId);
    return Promise.resolve();
  }

  getCurrentWindow() {
    this.methodCalled('getCurrentWindow');
    return Promise.resolve(this.currentWindow_);
  }

  moveTab(tabId, newIndex) {
    this.methodCalled('moveTab', [tabId, newIndex]);
    return Promise.resolve();
  }

  setCurrentWindow(currentWindow) {
    this.currentWindow_ = currentWindow;
  }

  trackThumbnailForTab(tabId) {
    this.methodCalled('trackThumbnailForTab', tabId);
  }
}
