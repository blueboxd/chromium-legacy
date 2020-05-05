// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// clang-format on

/** @interface */
export class AppearanceBrowserProxy {
  /** @return {!Promise<number>} */
  getDefaultZoom() {}

  /**
   * @param {string} themeId
   * @return {!Promise<!chrome.management.ExtensionInfo>} Theme info.
   */
  getThemeInfo(themeId) {}

  /** @return {boolean} Whether the current profile is supervised. */
  isSupervised() {}

  useDefaultTheme() {}

  // <if expr="is_linux and not chromeos">
  useSystemTheme() {}

  // </if>

  /**
   * @param {string} url The url of which to check validity.
   * @return {!Promise<boolean>}
   */
  validateStartupPage(url) {}
}

/**
 * @implements {AppearanceBrowserProxy}
 */
export class AppearanceBrowserProxyImpl {
  /** @override */
  getDefaultZoom() {
    return new Promise(function(resolve) {
      chrome.settingsPrivate.getDefaultZoom(resolve);
    });
  }

  /** @override */
  getThemeInfo(themeId) {
    return new Promise(function(resolve) {
      chrome.management.get(themeId, resolve);
    });
  }

  /** @override */
  isSupervised() {
    return loadTimeData.getBoolean('isSupervised');
  }

  /** @override */
  useDefaultTheme() {
    chrome.send('useDefaultTheme');
  }

  // <if expr="is_linux and not chromeos">
  /** @override */
  useSystemTheme() {
    chrome.send('useSystemTheme');
  }

  // </if>

  /** @override */
  validateStartupPage(url) {
    return sendWithPromise('validateStartupPage', url);
  }
}

addSingletonGetter(AppearanceBrowserProxyImpl);
