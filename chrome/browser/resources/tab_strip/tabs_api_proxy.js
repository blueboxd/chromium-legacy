// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * Must be kept in sync with TabNetworkState from
 * //chrome/browser/ui/tabs/tab_network_state.h.
 * @enum {number}
 */
export const TabNetworkState = {
  NONE: 0,
  WAITING: 1,
  LOADING: 2,
  ERROR: 3,
};

/**
 * @typedef {{
 *    active: boolean,
 *    blocked: boolean,
 *    crashed: boolean,
 *    favIconUrl: (string|undefined),
 *    id: number,
 *    index: number,
 *    isDefaultFavicon: boolean,
 *    networkState: !TabNetworkState,
 *    pinned: boolean,
 *    shouldHideThrobber: boolean,
 *    showIcon: boolean,
 *    title: string,
 *    url: string,
 * }}
 */
export let TabData;

/** @typedef {!Tab} */
let ExtensionsApiTab;

export class TabsApiProxy {
  /**
   * @param {number} tabId
   * @return {!Promise<!ExtensionsApiTab>}
   */
  activateTab(tabId) {
    return new Promise(resolve => {
      chrome.tabs.update(tabId, {active: true}, resolve);
    });
  }

  /**
   * @return {!Promise<!Array<!TabData>>}
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
   * @return {!Promise<!ExtensionsApiTab>}
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
