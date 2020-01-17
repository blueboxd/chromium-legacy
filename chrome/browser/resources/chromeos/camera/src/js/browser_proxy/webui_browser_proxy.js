// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {BrowserProxy} from './browser_proxy_interface.js';

/* eslint-disable new-cap */

/** @throws {Error} */
function NOTIMPLEMENTED() {
  throw Error('Browser proxy method not implemented!');
}

/**
 * The WebUI implementation of the CCA's interaction with the browser.
 * @implements {BrowserProxy}
 */
class WebUIBrowserProxy {
  /** @override */
  getVolumeList(callback) {
    NOTIMPLEMENTED();
  }

  /** @override */
  requestFileSystem(options, callback) {
    NOTIMPLEMENTED();
  }

  /** @override */
  localStorageGet(keys, callback) {
    let sanitizedKeys = [];
    if (typeof keys === 'string') {
      sanitizedKeys = [keys];
    } else if (Array.isArray(keys)) {
      sanitizedKeys = keys;
    } else if (keys !== null && typeof keys === 'object') {
      sanitizedKeys = Object.keys(keys);
    } else {
      throw new Error('WebUI localStorageGet() cannot be run with ' + keys);
    }

    const result = {};
    for (const key of sanitizedKeys) {
      let value = window.localStorage.getItem(key);
      if (value !== null) {
        value = JSON.parse(value);
      }
      result[key] = value === null ? {} : value;
    }

    callback(result);
  }

  /** @override */
  localStorageSet(items, callback) {
    for (const [key, val] of Object.entries(items)) {
      window.localStorage.setItem(key, JSON.stringify(val));
    }
    if (callback) {
      callback();
    }
  }

  /** @override */
  localStorageRemove(items, callback) {
    if (typeof items === 'string') {
      items = [items];
    }
    for (const key of items) {
      window.localStorage.removeItem(key);
    }
    if (callback) {
      callback();
    }
  }
}

export const browserProxy = new WebUIBrowserProxy();

/* eslint-enable new-cap */
