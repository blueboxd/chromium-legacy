// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for proxy.
 */
cca.proxy = cca.proxy || {};

(function() {
/* eslint-disable new-cap */

/** @throws {Error} */
function NOTIMPLEMENTED() {
  throw Error('Browser proxy method not implemented!');
}

/**
 * The WebUI implementation of the CCA's interaction with the browser.
 * @implements {cca.proxy.BrowserProxy}
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
    NOTIMPLEMENTED();
  }

  /** @override */
  localStorageSet(items, callback) {
    NOTIMPLEMENTED();
  }

  /** @override */
  localStorageRemove(items, callback) {
    NOTIMPLEMENTED();
  }
}

/* eslint-enable new-cap */

/**
 * Namespace for browser functions.
 * @type {cca.proxy.BrowserProxy}
 */
cca.proxy.browserProxy = new WebUIBrowserProxy();
})();
