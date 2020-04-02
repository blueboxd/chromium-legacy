// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the ambient mode section to interact
 * with the browser.
 */

cr.define('settings', function() {
  /** @interface */
  class AmbientModeBrowserProxy {
    onAmbientModePageReady() {}
  }

  /** @implements {settings.AmbientModeBrowserProxy} */
  class AmbientModeBrowserProxyImpl {
    /** @override */
    onAmbientModePageReady() {
      chrome.send('onAmbientModePageReady');
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(AmbientModeBrowserProxyImpl);

  // #cr_define_end
  return {
    AmbientModeBrowserProxy: AmbientModeBrowserProxy,
    AmbientModeBrowserProxyImpl: AmbientModeBrowserProxyImpl,
  };
});
