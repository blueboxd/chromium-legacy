// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used to open a URL in a new tab.
 * the browser.
 */

cr.define('settings', function() {
  /** @interface */
  class OpenWindowProxy {
    /**
     * Opens the specified URL in a new tab.
     * @param {string} url
     */
    openURL(url) {}
  }

  /** @implements {settings.OpenWindowProxy} */
  class OpenWindowProxyImpl {
    /** @override */
    openURL(url) {
      window.open(url);
    }
  }

  cr.addSingletonGetter(OpenWindowProxyImpl);

  return {OpenWindowProxy, OpenWindowProxyImpl};
});
