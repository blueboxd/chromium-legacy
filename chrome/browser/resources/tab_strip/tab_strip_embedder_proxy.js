// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';

export class TabStripEmbedderProxy {
  /** @return {boolean} */
  isVisible() {
    return document.visibilityState === 'visible';
  }

  /**
   * @return {!Promise<!Object<string, string>>} Object with CSS variables
   *     as keys and rgba strings as values
   */
  getColors() {
    return sendWithPromise('getThemeColors');
  }

  /**
   * @return {!Promise<!Object<string, string>>} Object with CSS variables
   *     as keys and pixel lengths as values
   */
  getLayout() {
    return sendWithPromise('getLayout');
  }

  observeThemeChanges() {
    chrome.send('observeThemeChanges');
  }

  /**
   * @param {string} groupId
   * @param {number} locationX
   * @param {number} locationY
   * @param {number} width
   * @param {number} height
   */
  showEditDialogForGroup(groupId, locationX, locationY, width, height) {
    chrome.send(
        'showEditDialogForGroup',
        [groupId, locationX, locationY, width, height]);
  }

  /**
   * @param {number} tabId
   * @param {number} locationX
   * @param {number} locationY
   */
  showTabContextMenu(tabId, locationX, locationY) {
    chrome.send('showTabContextMenu', [tabId, locationX, locationY]);
  }

  /**
   * @param {number} locationX
   * @param {number} locationY
   */
  showBackgroundContextMenu(locationX, locationY) {
    chrome.send('showBackgroundContextMenu', [locationX, locationY]);
  }

  closeContainer() {
    chrome.send('closeContainer');
  }

  /** @param {number} durationMs Activation duration time in ms. */
  reportTabActivationDuration(durationMs) {
    chrome.send('reportTabActivationDuration', [durationMs]);
  }

  /**
   * @param {number} tabCount Number of tabs.
   * @param {number} durationMs Activation duration time in ms.
   */
  reportTabDataReceivedDuration(tabCount, durationMs) {
    chrome.send('reportTabDataReceivedDuration', [tabCount, durationMs]);
  }

  /**
   * @param {number} tabCount Number of tabs.
   * @param {number} durationMs Creation duration time in ms.
   */
  reportTabCreationDuration(tabCount, durationMs) {
    chrome.send('reportTabCreationDuration', [tabCount, durationMs]);
  }
}

addSingletonGetter(TabStripEmbedderProxy);
