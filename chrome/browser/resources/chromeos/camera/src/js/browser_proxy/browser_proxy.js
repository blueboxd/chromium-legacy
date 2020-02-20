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

  /** @override */
  isCrashReportingEnabled() {
    return util.promisify(chrome.metricsPrivate.getIsCrashReportingEnabled)();
  }

  /** @override */
  async openGallery(file) {
    const id = 'nlkncpkkdoccmpiclbokaimcnedabhhm|app|open';
    try {
      const result = await util.promisify(
          chrome.fileManagerPrivate.executeTask)(id, [file]);
      if (result !== 'message_sent') {
        console.warn('Unable to open picture: ' + result);
      }
    } catch (e) {
      console.warn('Unable to open picture', e);
      return;
    }
  }

  /** @override */
  openInspector(type) {
    chrome.fileManagerPrivate.openInspector(type);
  }

  /** @override */
  getAppId() {
    return chrome.runtime.id;
  }

  /** @override */
  getAppVersion() {
    return chrome.runtime.getManifest().version;
  }

  /** @override */
  addOnMessageExternalListener(listener) {
    chrome.runtime.onMessageExternal.addListener(listener);
  }

  /** @override */
  addOnConnectExternalListener(listener) {
    chrome.runtime.onConnectExternal.addListener(listener);
  }

  /** @override */
  sendMessage(extensionId, message) {
    chrome.runtime.sendMessage(extensionId, message);
  }
}

export const browserProxy = new ChromeAppBrowserProxy();
