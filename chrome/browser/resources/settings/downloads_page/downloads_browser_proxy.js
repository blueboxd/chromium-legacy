// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  class DownloadsBrowserProxy {
    initializeDownloads() {}
    selectDownloadLocation() {}
    resetAutoOpenFileTypes() {}
    // <if expr="chromeos">
    /**
     * @param {string} path path to sanitze.
     * @return {!Promise<string>} string to display in UI.
     */
    getDownloadLocationText(path) {}
    // </if>
  }

  /**
   * @implements {settings.DownloadsBrowserProxy}
   */
  class DownloadsBrowserProxyImpl {
    /** @override */
    initializeDownloads() {
      chrome.send('initializeDownloads');
    }

    /** @override */
    selectDownloadLocation() {
      chrome.send('selectDownloadLocation');
    }

    /** @override */
    resetAutoOpenFileTypes() {
      chrome.send('resetAutoOpenFileTypes');
    }

    // <if expr="chromeos">
    /** @override */
    getDownloadLocationText(path) {
      return cr.sendWithPromise('getDownloadLocationText', path);
    }
    // </if>
  }

  cr.addSingletonGetter(DownloadsBrowserProxyImpl);

  // #cr_define_end
  return {
    DownloadsBrowserProxy: DownloadsBrowserProxy,
    DownloadsBrowserProxyImpl: DownloadsBrowserProxyImpl,
  };
});
