// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * @fileoverview 'os-settings-search-box' is the container for the search input
 * and settings search results.
 */

/**
 * Fake function to simulate async SettingsSearchHandler Search().
 * TODO(crbug/1056909): Remove once Settings Search Handler mojo API is ready.
 * @param {string} query The query used to fetch results.
 * @return {!Promise<!Array<string>>} A promise that will resolve with an array
 *     of search results.
 */
function fakeSettingsSearchHandlerSearch(query) {
  /**
   * @param {number} min The lower bound integer.
   * @param {number} max The upper bound integer.
   * @return {number} A random integer between min and max inclusive.
   */
  function getRandomInt(min, max) {
    min = Math.ceil(min);
    max = Math.floor(max);
    return Math.floor(Math.random() * (max - min + 1)) + min;
  }

  const fakeRandomResults = [
    'bluetooth', 'wifi', 'language', 'people', 'personalization', 'security',
    'touchpad', 'keyboard', 'passcode'
  ];
  fakeRandomResults.sort(() => Math.random() - 0.5);
  return new Promise(resolve => {
    setTimeout(() => {
      resolve(fakeRandomResults.splice(
          0, getRandomInt(1, fakeRandomResults.length - 1)));
    }, 0);
  });
}

Polymer({
  is: 'os-settings-search-box',

  properties: {
    // True when the toolbar is displaying in narrow mode.
    // TODO(hsuregan): Change narrow to isNarrow here and associated elements.
    narrow: {
      type: Boolean,
      reflectToAttribute: true,
    },

    // Controls whether the search field is shown.
    showingSearch: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true,
    },

    // Value is proxied through to cr-toolbar-search-field. When true,
    // the search field will show a processing spinner.
    spinnerActive: Boolean,

    // The currently selected search result associated with a
    // <os-search-result-row>. This property is bound to <iron-list>.
    // Note that when a item is selected, it's associated <os-search-result-row>
    // is not focused() at the same time unless it is explicitly clicked/tapped.
    selectedItem: {
      // TODO(crbug/1056909): Change the type from String to Mojo result type.
      type: String,
      notify: true,
    },

    /**
     * Passed into <iron-list>, one of which is always the selectedItem.
     * @private {!Array<string>}
     */
    searchResults_: {
      type: Array,
      notify: true,
      observer: 'selectFirstRow_',
    },

    /** @private */
    shouldShowDropdown_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowDropdown_(searchResults_)',
    },

    /**
     * Used by FocusRowBehavior to track the last focused element inside a
     * <os-search-result-row> with the attribute 'focus-row-control'.
     * @private {HTMLElement}
     */
    lastFocused_: Object,

    /**
     * Used by FocusRowBehavior to track if the list has been blurred.
     * @private
     */
    listBlurred_: Boolean,
  },

  /**
   * Mojo OS Settings Search handler used to fetch search results.
   * TODO(crbug/1056909): Set in ready() when Mojo bindings are ready or
   * eliminate altogether along with fake JS file.
   * @private {*}
   */
  searchHandler_: undefined,

  listeners: {
    'blur': 'onBlur_',
    'keydown': 'onKeyDown_',
    'search-changed': 'fetchSearchResults_',
  },

  /** @private */
  attached() {
    const toolbarSearchField = this.$.search;
    const searchInput = toolbarSearchField.getSearchInput();
    searchInput.addEventListener(
        'focus', this.onSearchInputFocused_.bind(this));
  },

  /**
   * @param {*} searchHandler
   */
  setSearchHandlerForTesting(searchHandler) {
    this.searchHandler_ = searchHandler;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowDropdown_() {
    return this.searchResults_.length !== 0;
  },

  /** @private */
  fetchSearchResults_() {
    const query = this.$.search.getSearchInput().value;
    if (query === '') {
      this.searchResults_ = [];
      return;
    }

    this.spinnerActive = true;

    if (!this.searchHandler_) {
      // TODO(crbug/1056909): Remove block when mojo interface is ready.
      fakeSettingsSearchHandlerSearch(query).then(results => {
        this.onSearchResultsReceived_(query, results);
      });
      return;
    }

    this.searchHandler_.search(query).then(results => {
      this.onSearchResultsReceived_(query, results);
    });
  },

  /**
   * Updates search results UI when settings search results are fetched.
   * @param {string} query The string used to find search results.
   * @param {!Array<string>} results Array of search results.
   * @private
   */
  onSearchResultsReceived_(query, results) {
    if (query !== this.$.search.getSearchInput().value) {
      // Received search results are invalid as the query has since changed.
      return;
    }

    this.spinnerActive = false;
    this.lastFocused_ = null;
    this.searchResults_ = results;
    settings.recordSearch();
  },

  /** @private */
  onBlur_() {
    // The user has clicked a region outside the search box or the input has
    // been blurred; close the dropdown regardless if there are searchResults_.
    this.$.searchResults.close();
  },

  /** @private */
  onSearchInputFocused_() {
    this.lastFocused_ = null;

    if (this.shouldShowDropdown_) {
      // Restore previous results instead of re-fetching.
      this.$.searchResults.open();
      return;
    }

    this.fetchSearchResults_();
  },

  /**
   * @param {string} item The search result item.
   * @return {boolean} True if the item is selected.
   * @private
   */
  isItemSelected_(item) {
    return this.searchResults_.indexOf(item) ===
        this.searchResults_.indexOf(this.selectedItem);
  },

  /**
   * Returns the correct tab index since <iron-list>'s default tabIndex property
   * does not automatically add selectedItem's <os-search-result-row> to the
   * default navigation flow, unless the user explicitly clicks on the row.
   * @param {string} item The search result item.
   * @return {number} A 0 if the row should be in the navigation flow, or a -1
   *     if the row should not be in the navigation flow.
   * @private
   */
  getRowTabIndex_(item) {
    return this.isItemSelected_(item) ? 0 : -1;
  },

  /** @private */
  selectFirstRow_() {
    if (!this.shouldShowDropdown_) {
      return;
    }

    this.selectedItem = this.searchResults_[0];
  },

  /**
   * @param {string} key The string associated with a key.
   * @private
   */
  selectRowViaKeys_(key) {
    assert(key === 'ArrowDown' || key === 'ArrowUp' || key === 'Tab');

    assert(!!this.selectedItem, 'There should be a selected item already.');

    // Select the new item.
    const selectedRowIndex = this.searchResults_.indexOf(this.selectedItem);
    const numRows = this.searchResults_.length;
    const delta = key === 'ArrowUp' ? -1 : 1;
    const indexOfNewRow = (numRows + selectedRowIndex + delta) % numRows;
    this.selectedItem = this.searchResults_[indexOfNewRow];

    // If a row is focused, ensure it is the selectedResult's row.
    if (this.lastFocused_) {
      const rowEls = Array.from(
          this.$.searchResultList.querySelectorAll('os-search-result-row'));
      // Calling focus() on a <os-search-result-row> focuses the element within
      // containing the attribute 'focus-row-control'.
      rowEls[indexOfNewRow].focus();
    }
  },

  /**
   * Keydown handler to specify how enter-key, arrow-up key, and arrow-down-key
   * interacts with search results in the dropdown.
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (!this.shouldShowDropdown_) {
      // No action should be taken if there are no search results.
      return;
    }

    if (e.key === 'Enter') {
      // TODO(crbug/1056909): Take action on selected row.
      return;
    }

    if (e.key === 'Tab') {
      if (this.lastFocused_) {
        // Prevent continuous tabbing from leaving search result
        e.preventDefault();
        this.selectRowViaKeys_(e.key);
      }
      return;
    }

    if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      // Do not impact the position of <cr-toolbar-search-field>'s caret.
      e.preventDefault();
      this.selectRowViaKeys_(e.key);
      return;
    }
  },
});
})();
