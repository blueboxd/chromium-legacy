// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * Type definition of AndroidAppsInfo entry. |playStoreEnabled| indicates that
 * Play Store is enabled. |settingsAppAvailable| indicates that Android settings
 * app is registered in the system.
 * @typedef {{
 *   playStoreEnabled: boolean,
 *   settingsAppAvailable: boolean,
 * }}
 * @see chrome/browser/ui/webui/settings/chromeos/android_apps_handler.cc
 */
export let AndroidAppsInfo;

/**
 * An object containing messages for web permissisions origin
 * and the messages multidevice feature state.
 *
 * @typedef {{origin: string,
 *            enabled: boolean}}
 */
export let AndroidSmsInfo;

/** @interface */
class AndroidInfoBrowserProxy {
  /**
   * Returns android messages info with messages feature state
   * and messages for web permissions origin.
   * @return {!Promise<!AndroidSmsInfo>} Android SMS Info
   */
  getAndroidSmsInfo() {}

  requestAndroidAppsInfo() {}
}

/**
 * @implements {AndroidInfoBrowserProxy}
 */
export class AndroidInfoBrowserProxyImpl {
  /** @override */
  getAndroidSmsInfo() {
    return sendWithPromise('getAndroidSmsInfo');
  }

  /** @override */
  requestAndroidAppsInfo() {
    chrome.send('requestAndroidAppsInfo');
  }
}

addSingletonGetter(AndroidInfoBrowserProxyImpl);
