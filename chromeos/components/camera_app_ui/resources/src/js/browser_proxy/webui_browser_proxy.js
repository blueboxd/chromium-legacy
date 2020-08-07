// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {BackgroundOps} from '../background_ops.js';
import {assert} from '../chrome_util.js';
import {NotImplementedError} from '../error.js';
import {Intent} from '../intent.js';
import {NativeDirectoryEntry} from '../models/native_file_system_entry.js';
import {PerfLogger} from '../perf.js';

// eslint-disable-next-line no-unused-vars
import {BrowserProxy} from './browser_proxy_interface.js';

/**
 * The WebUI implementation of the CCA's interaction with the browser.
 * @implements {BrowserProxy}
 */
class WebUIBrowserProxy {
  /** @override */
  async requestEnumerateDevicesPermission() {
    // No operation here since the permission is automatically granted for
    // the chrome:// scheme.
    return true;
  }

  /** @override */
  async getExternalDir() {
    return new Promise((resolve) => {
      const launchQueue = window.launchQueue;
      assert(launchQueue !== undefined);
      launchQueue.setConsumer((launchParams) => {
        assert(launchParams.files.length > 0);
        const dir =
            /** @type {!FileSystemDirectoryHandle} */ (launchParams.files[0]);
        assert(dir.kind === 'directory');
        resolve(new NativeDirectoryEntry(dir));
      });
    });
  }

  /** @override */
  async localStorageGet(keys) {
    let result = {};
    let sanitizedKeys = [];
    if (typeof keys === 'string') {
      sanitizedKeys = [keys];
    } else if (Array.isArray(keys)) {
      sanitizedKeys = keys;
    } else if (keys !== null && typeof keys === 'object') {
      sanitizedKeys = Object.keys(keys);

      // If any key does not exist, use the default value specified in the
      // input.
      result = Object.assign({}, keys);
    } else {
      throw new Error('WebUI localStorageGet() cannot be run with ' + keys);
    }

    for (const key of sanitizedKeys) {
      const value = window.localStorage.getItem(key);
      if (value !== null) {
        result[key] = JSON.parse(value);
      } else if (result[key] === undefined) {
        // For key that does not exist and does not have a default value, set it
        // to null.
        result[key] = null;
      }
    }
    return result;
  }

  /** @override */
  async localStorageSet(items) {
    for (const [key, val] of Object.entries(items)) {
      window.localStorage.setItem(key, JSON.stringify(val));
    }
  }

  /** @override */
  async localStorageRemove(items) {
    if (typeof items === 'string') {
      items = [items];
    }
    for (const key of items) {
      window.localStorage.removeItem(key);
    }
  }

  /** @override */
  async getBoard() {
    throw new NotImplementedError();
  }

  /** @override */
  getI18nMessage(name, substitutions = undefined) {
    throw new NotImplementedError();
  }

  /** @override */
  async isCrashReportingEnabled() {
    throw new NotImplementedError();
  }

  /** @override */
  async openGallery(file) {
    throw new NotImplementedError();
  }

  /** @override */
  openInspector(type) {
    throw new NotImplementedError();
  }

  /** @override */
  getAppId() {
    throw new NotImplementedError();
  }

  /** @override */
  getAppVersion() {
    throw new NotImplementedError();
  }

  /** @override */
  addOnMessageExternalListener(listener) {
    throw new NotImplementedError();
  }

  /** @override */
  addOnConnectExternalListener(listener) {
    throw new NotImplementedError();
  }

  /** @override */
  sendMessage(extensionId, message) {
    throw new NotImplementedError();
  }

  /** @override */
  addDummyHistoryIfNotAvailable() {
    // no-ops
  }

  /** @override */
  isMp4RecordingEnabled() {
    return false;
  }

  /** @override */
  getBackgroundOps() {
    // TODO(980846): Refactor after migrating to SWA since there is no
    // background page for SWA.
    const perfLogger = new PerfLogger();
    const url = window.location.href;
    const intent = url.includes('intent') ? Intent.create(new URL(url)) : null;
    return /** @type {BackgroundOps} */ ({
      bindForegroundOps: (ops) => {},
      getIntent: () => intent,
      getPerfLogger: () => perfLogger,
      getTestingErrorCallback: () => null,
      notifyActivation: () => {},
      notifySuspension: () => {},
    });
  }

  /** @override */
  isFullscreenOrMaximized() {
    // TODO(980846): Implement the fullscreen monitor.
    return false;
  }

  /** @override */
  fitWindow() {
    // TODO(980846): Remove the method once we migrate to SWA.
  }

  /** @override */
  showWindow() {
    // TODO(980846): Remove the method once we migrate to SWA.
  }

  /** @override */
  hideWindow() {
    // TODO(980846): Remove the method once we migrate to SWA.
  }

  /** @override */
  isMinimized() {
    // TODO(980846): Implement the minimization monitor.
    return false;
  }

  /** @override */
  addOnMinimizedListener(listener) {
    // TODO(980846): Implement the minimization monitor.
  }
}

export const browserProxy = new WebUIBrowserProxy();

/* eslint-enable new-cap */
