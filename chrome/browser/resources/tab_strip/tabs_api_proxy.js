// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

export class TabsApiProxy {
  /**
   * @param {number} tabId
   * @return {!Promise<!Tab>}
   */
  activateTab(tabId) {
    return new Promise(resolve => {
      chrome.tabs.update(tabId, {active: true}, resolve);
    });
  }

  /**
   * @return {!Promise<!Array<!Tab>>}
   */
  getTabs() {
    return sendWithPromise('getTabs');
  }

  /**
   * @param {number} tabId
   * @return {!Promise}
   */
  closeTab(tabId) {
    return new Promise(resolve => {
      chrome.tabs.remove(tabId, resolve);
    });
  }

  /**
   * @param {number} tabId
   * @param {number} newIndex
   * @return {!Promise<!Tab>}
   */
  moveTab(tabId, newIndex) {
    return new Promise(resolve => {
      chrome.tabs.move(tabId, {index: newIndex}, tab => {
        resolve(tab);
      });
    });
  }

  /**
   * @param {number} tabId
   */
  trackThumbnailForTab(tabId) {
    chrome.send('addTrackedTab', [tabId]);
  }
}

addSingletonGetter(TabsApiProxy);
