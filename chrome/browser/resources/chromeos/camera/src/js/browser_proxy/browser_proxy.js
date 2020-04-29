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
  async getVolumeList() {
    try {
      // Await here to handle the case if it is rejected.
      return await util.promisify(chrome.fileSystem.getVolumeList)();
    } catch (e) {
      console.error('Failed to get volume list', e);
      return null;
    }
  }

  /** @override */
  async requestFileSystem(options) {
    try {
      // Await here to handle the case if it is rejected.
      return await util.promisify(chrome.fileSystem.requestFileSystem)(options);
    } catch (e) {
      console.error('Failed to request file system', e);
      return null;
    }
  }

  /** @override */
  localStorageGet(keys) {
    return util.promisify(chrome.storage.local.get.bind(chrome.storage.local))(
        keys);
  }

  /** @override */
  localStorageSet(items) {
    return util.promisify(chrome.storage.local.set.bind(chrome.storage.local))(
        items);
  }

  /** @override */
  localStorageRemove(items) {
    return util.promisify(
        chrome.storage.local.remove.bind(chrome.storage.local))(items);
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

  /** @override */
  addDummyHistoryIfNotAvailable() {
    // Since GA will use history.length to generate hash but it is not available
    // in platform apps, set it to 1 manually.
    window.history.length = 1;
  }

  /** @override */
  isMp4RecordingEnabled() {
    return true;
  }
}

export const browserProxy = new ChromeAppBrowserProxy();
