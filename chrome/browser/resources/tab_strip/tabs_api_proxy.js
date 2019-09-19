// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class TabsApiProxy {
  constructor() {
    /** @type {!Object<string, !ChromeEvent>} */
    this.callbackRouter = {
      onActivated: chrome.tabs.onActivated,
      onCreated: chrome.tabs.onCreated,
      onMoved: chrome.tabs.onMoved,
      onRemoved: chrome.tabs.onRemoved,
      onUpdated: chrome.tabs.onUpdated,
    };
  }

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
   * @return {!Promise<!ChromeWindow>}
   */
  getCurrentWindow() {
    const options = {
      populate: true,           // populate window data with tabs data
      windowTypes: ['normal'],  // prevent devtools from being returned
    };
    return new Promise(resolve => {
      chrome.windows.getCurrent(options, currentWindow => {
        resolve(currentWindow);
      });
    });
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
