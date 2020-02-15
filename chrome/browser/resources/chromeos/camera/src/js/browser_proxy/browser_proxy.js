// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as util from '../chrome_util.js';
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

  /** @override */
  async checkMigrated() {
    const values = await util.promisify(chrome.chromeosInfoPrivate.get)(
        ['cameraMediaConsolidated']);
    return values['cameraMediaConsolidated'];
  }

  /** @override */
  async doneMigrate() {
    chrome.chromeosInfoPrivate.set('cameraMediaConsolidated', true);
  }

  /** @override */
  async getBoard() {
    const values =
        await util.promisify(chrome.chromeosInfoPrivate.get)(['board']);
    return values['board'];
  }

  /** @override */
  getI18nMessage(name, substitutions = undefined) {
    return chrome.i18n.getMessage(name, substitutions);
  }

  /** @override */
  addOnLockChangeListener(callback) {
    chrome.idle.onStateChanged.addListener((newState) => {
      callback(newState === 'locked');
    });
  }
}

export const browserProxy = new ChromeAppBrowserProxy();
