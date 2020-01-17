// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {BrowserProxy} from './browser_proxy_interface.js';

/**
 * The Chrome App implementation of the CCA's interaction with the browser.
 * @implements {BrowserProxy}
 */
class ChromeAppBrowserProxy {
  /** @override */
  getVolumeList(callback) {
    chrome.fileSystem.getVolumeList(callback);
  }

  /** @override */
  requestFileSystem(options, callback) {
    chrome.fileSystem.requestFileSystem(options, callback);
  }

  /** @override */
  localStorageGet(keys, callback) {
    chrome.storage.local.get(keys, callback);
  }

  /** @override */
  localStorageSet(items, callback) {
    chrome.storage.local.set(items, callback);
  }

  /** @override */
  localStorageRemove(items, callback) {
    chrome.storage.local.remove(items, callback);
  }
}

export const browserProxy = new ChromeAppBrowserProxy();
