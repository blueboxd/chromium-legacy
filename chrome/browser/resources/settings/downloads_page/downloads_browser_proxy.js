// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
export class DownloadsBrowserProxy {
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
 * @implements {DownloadsBrowserProxy}
 */
export class DownloadsBrowserProxyImpl {
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
    return sendWithPromise('getDownloadLocationText', path);
  }
  // </if>
}

addSingletonGetter(DownloadsBrowserProxyImpl);
