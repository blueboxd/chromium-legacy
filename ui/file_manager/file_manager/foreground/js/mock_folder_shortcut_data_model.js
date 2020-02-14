// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Mock FolderShortcutDataModel.
 */
class MockFolderShortcutDataModel extends cr.ui.ArrayDataModel {
  /**
   * @param {!Array} array
   */
  constructor(array) {
    super(array);
  }

  /**
   * @return {!FolderShortcutsDataModel}
   * @public
   */
  asFolderShortcutsDataModel() {
    const instance = /** @type {!Object} */ (this);
    return /** @type {!FolderShortcutsDataModel} */ (instance);
  }

  /**
   * Mock function for FolderShortcutDataModel.compare().
   * @param {MockEntry} a First parameter to be compared.
   * @param {MockEntry} b Second parameter to be compared with.
   * @return {number} Negative if a < b, positive if a > b, or zero if a == b.
   */
  compare(a, b) {
    return a.fullPath.localeCompare(b.fullPath);
  }
}
