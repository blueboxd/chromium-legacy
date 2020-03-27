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

    // String to be displayed as a result in the UI.
    searchResultText: String,
  },
});
