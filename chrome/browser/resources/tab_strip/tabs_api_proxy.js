// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const CloseTabAction = {
  CLOSE_BUTTON: 0,
  SWIPED_TO_CLOSE: 1,
};

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
 * Must be kept in sync with TabAlertState from
 * //chrome/browser/ui/tabs/tab_utils.h
 * @enum {number}
 */
export const TabAlertState = {
  MEDIA_RECORDING: 0,
  TAB_CAPTURING: 1,
  AUDIO_PLAYING: 2,
  AUDIO_MUTING: 3,
  BLUETOOTH_CONNECTED: 4,
  USB_CONNECTED: 5,
  HID_CONNECTED: 6,
  SERIAL_CONNECTED: 7,
  PIP_PLAYING: 8,
  DESKTOP_CAPTURING: 9,
  VR_PRESENTING_IN_HEADSET: 10,
};

/**
 * @typedef {{
 *    active: boolean,
 *    alertStates: !Array<!TabAlertState>,
 *    blocked: boolean,
 *    crashed: boolean,
 *    favIconUrl: (string|undefined),
 *    groupId: (string|undefined),
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

/**
 * @typedef {{
 *   color: string,
 *   textColor: string,
 *   title: string,
 * }}
 */
export let TabGroupVisualData;

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

  createNewTab() {
    chrome.send('createNewTab');
  }

  /**
   * @return {!Promise<!Object<!TabGroupVisualData>>} Object of group IDs as
   *     strings mapped to their visual data.
   */
  getGroupVisualData() {
    return sendWithPromise('getGroupVisualData');
  }

  /**
   * @return {!Promise<!Array<!TabData>>}
   */
  getTabs() {
    return sendWithPromise('getTabs');
  }

  /**
   * @param {number} tabId
   * @param {!CloseTabAction} closeTabAction
   */
  closeTab(tabId, closeTabAction) {
    chrome.send(
        'closeTab', [tabId, closeTabAction === CloseTabAction.SWIPED_TO_CLOSE]);
    chrome.metricsPrivate.recordEnumerationValue(
        'WebUITabStrip.CloseTabAction', closeTabAction,
        Object.keys(CloseTabAction).length);
  }

  /**
   * @param {number} tabId
   * @param {string} groupId
   */
  groupTab(tabId, groupId) {
    chrome.send('groupTab', [tabId, groupId]);
  }

  /**
   * @param {string} groupId
   * @param {number} newIndex
   */
  moveGroup(groupId, newIndex) {
    chrome.send('moveGroup', [groupId, newIndex]);
  }

  /**
   * @param {number} tabId
   * @param {number} newIndex
   */
  moveTab(tabId, newIndex) {
    chrome.send('moveTab', [tabId, newIndex]);
  }

  /**
   * @param {number} tabId
   * @param {boolean} thumbnailTracked
   */
  setThumbnailTracked(tabId, thumbnailTracked) {
    chrome.send('setThumbnailTracked', [tabId, thumbnailTracked]);
  }

  /** @param {number} tabId */
  ungroupTab(tabId) {
    chrome.send('ungroupTab', [tabId]);
  }
}

addSingletonGetter(TabsApiProxy);
