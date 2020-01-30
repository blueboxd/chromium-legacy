// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

export class TestTabsApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'activateTab',
      'closeTab',
      'createNewTab',
      'getGroupVisualData',
      'getTabs',
      'groupTab',
      'moveGroup',
      'moveTab',
      'setThumbnailTracked',
      'ungroupTab',
    ]);

    this.groupVisualData_;
    this.tabs_;
  }

  activateTab(tabId) {
    this.methodCalled('activateTab', tabId);
    return Promise.resolve({active: true, id: tabId});
  }

  closeTab(tabId, closeTabAction) {
    this.methodCalled('closeTab', [tabId, closeTabAction]);
    return Promise.resolve();
  }

  createNewTab() {
    this.methodCalled('createNewTab');
  }

  getGroupVisualData() {
    this.methodCalled('getGroupVisualData');
    return Promise.resolve(this.groupVisualData_);
  }

  getTabs() {
    this.methodCalled('getTabs');
    return Promise.resolve(this.tabs_.slice());
  }

  groupTab(tabId, groupId) {
    this.methodCalled('groupTab', [tabId, groupId]);
  }

  moveGroup(groupId, newIndex) {
    this.methodCalled('moveGroup', [groupId, newIndex]);
  }

  moveTab(tabId, windowId, newIndex) {
    this.methodCalled('moveTab', [tabId, windowId, newIndex]);
    return Promise.resolve();
  }

  setGroupVisualData(groupVisualData) {
    this.groupVisualData_ = groupVisualData;
  }

  setTabs(tabs) {
    this.tabs_ = tabs;
  }

  setThumbnailTracked(tabId, thumbnailTracked) {
    this.methodCalled('setThumbnailTracked', [tabId, thumbnailTracked]);
  }

  ungroupTab(tabId) {
    this.methodCalled('ungroupTab', [tabId]);
  }
}
