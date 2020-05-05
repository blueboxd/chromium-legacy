// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * An object describing a source browser profile that may be imported.
 * The structure of this data must be kept in sync with C++ ImportDataHandler.
 * @typedef {{
 *   name: string,
 *   index: number,
 *   profileName: string,
 *   history: boolean,
 *   favorites: boolean,
 *   passwords: boolean,
 *   search: boolean,
 *   autofillFormData: boolean,
 * }}
 */
export let BrowserProfile;

/**
 * @enum {string}
 * These string values must be kept in sync with the C++ ImportDataHandler.
 */
export const ImportDataStatus = {
  INITIAL: 'initial',
  IN_PROGRESS: 'inProgress',
  SUCCEEDED: 'succeeded',
  FAILED: 'failed',
};

/** @interface */
export class ImportDataBrowserProxy {
  /**
   * Returns the source profiles available for importing from other browsers.
   * @return {!Promise<!Array<!BrowserProfile>>}
   */
  initializeImportDialog() {}

  /**
   * Starts importing data for the specified source browser profile. The C++
   * responds with the 'import-data-status-changed' WebUIListener event.
   * @param {number} sourceBrowserProfileIndex
   * @param {!Object<boolean>} types Which types of data to import.
   */
  importData(sourceBrowserProfileIndex, types) {}

  /**
   * Prompts the user to choose a bookmarks file to import bookmarks from.
   */
  importFromBookmarksFile() {}
}

/** @implements {ImportDataBrowserProxy} */
export class ImportDataBrowserProxyImpl {
  /** @override */
  initializeImportDialog() {
    return sendWithPromise('initializeImportDialog');
  }

  /** @override */
  importData(sourceBrowserProfileIndex, types) {
    chrome.send('importData', [sourceBrowserProfileIndex, types]);
  }

  /** @override */
  importFromBookmarksFile() {
    chrome.send('importFromBookmarksFile');
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
addSingletonGetter(ImportDataBrowserProxyImpl);
