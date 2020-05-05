// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Default Browser" section
 * to interact with the browser.
 */

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @typedef {{
 *   canBeDefault: boolean,
 *   isDefault: boolean,
 *   isDisabledByPolicy: boolean,
 *   isUnknownError: boolean,
 * }};
 */
export let DefaultBrowserInfo;

/** @interface */
export class DefaultBrowserBrowserProxy {
  /**
   * Get the initial DefaultBrowserInfo and begin sending updates to
   * 'settings.updateDefaultBrowserState'.
   * @return {!Promise<!DefaultBrowserInfo>}
   */
  requestDefaultBrowserState() {}

  /*
   * Try to set the current browser as the default browser. The new status of
   * the settings will be sent to 'settings.updateDefaultBrowserState'.
   */
  setAsDefaultBrowser() {}
}

/** @implements {DefaultBrowserBrowserProxy} */
export class DefaultBrowserBrowserProxyImpl {
  /** @override */
  requestDefaultBrowserState() {
    return sendWithPromise('requestDefaultBrowserState');
  }

  /** @override */
  setAsDefaultBrowser() {
    chrome.send('setAsDefaultBrowser');
  }
}

addSingletonGetter(DefaultBrowserBrowserProxyImpl);
