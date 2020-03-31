// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-search-result-row' is the container for one search result.
 */

Polymer({
  is: 'os-search-result-row',

  behaviors: [cr.ui.FocusRowBehavior],

  properties: {
    // Whether the search result row is selected.
    selected: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {!chromeos.settings.mojom.SearchResult} */
    searchResult: Object,
  },

  /**
   * @return {string} Exact string of the result to be displayed.
   */
  getResultText_() {
    // The C++ layer stores the text result as an array of 16 bit char codes,
    // so it must be converted to a JS String.
    return String.fromCharCode.apply(null, this.searchResult.resultText.data);
  },
});
