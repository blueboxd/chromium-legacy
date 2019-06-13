// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/** @type {!RegExp} */
const SANITIZE_REGEX = /[-[\]{}()*+?.,\\^$|#\s]/g;

Polymer({
  is: 'print-preview-search-box',

  behaviors: [CrSearchFieldBehavior],

  properties: {
    autofocus: Boolean,

    /** @type {?RegExp} */
    searchQuery: {
      type: Object,
      notify: true,
    },
  },

  listeners: {
    'search-changed': 'onSearchChanged_',
  },

  /**
   * The last search query.
   * @private {string}
   */
  lastString_: '',

  /** @return {!HTMLInputElement} */
  getSearchInput: function() {
    return this.$.searchInput;
  },

  focus: function() {
    this.$.searchInput.focus();
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new search.
   * @private
   */
  onSearchChanged_: function(e) {
    const safeQueryString = e.detail.trim().replace(SANITIZE_REGEX, '\\$&');
    if (safeQueryString === this.lastString_) {
      return;
    }

    this.lastString_ = safeQueryString;
    this.searchQuery = safeQueryString.length > 0 ?
        new RegExp(`(${safeQueryString})`, 'i') :
        null;
  },

  /** @private */
  onClearClick_: function() {
    this.setValue('');
    this.$.searchInput.focus();
  },
});
})();
