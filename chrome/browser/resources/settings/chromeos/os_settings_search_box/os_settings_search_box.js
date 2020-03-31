// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

const mojom = chromeos.settings.mojom;

/**
 * @fileoverview 'os-settings-search-box' is the container for the search input
 * and settings search results.
 */

/**
 * Fake function to simulate async SettingsSearchHandler Search().
 * TODO(crbug/1056909): Remove once Settings Search is complete.
 * @param {string} query The query used to fetch results.
 * @return {Promise<!Array<mojom.SearchResult>>} A promise resolving results.
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

  /**
   * @param {*} resultArr Result stored in arr like [text, path, icon].
   * @return {!mojom.SearchResult} Search result entry.
   */
  function generateFakeResult(resultArr) {
    return /** @type {!mojom.SearchResult} */ ({
      resultText: {
        data: Array.from(resultArr[0], c => c.charCodeAt()),
      },
      urlPathWithParameters: resultArr[1],
      icon: resultArr[2],
    });
  }

  const Icon = mojom.SearchResultIcon;
  const fakeRandomResults = [
    ['bluetooth devices', 'bluetoothDevices', Icon.kWifi],
    ['wifi', 'networks?type=WiFi', Icon.kWifi],
    ['languages', 'languages/details', Icon.kWifi],
    ['people', 'people', Icon.kWifi],
    ['security', 'privacy', Icon.kWifi],
    ['personalization', 'personalization', Icon.kWifi],
    ['keyboard', 'keyboard-overlay', Icon.kWifi],
    ['touchpad', 'pointer-overlay', Icon.kWifi],
    ['lock screen', 'lockScreen', Icon.kWifi],
    ['time zone', 'dateTime/timeZone', Icon.kWifi],
  ].map(result => generateFakeResult(result));

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

    /**
     * The currently selected search result associated with an
     * <os-search-result-row>. This property is bound to the <iron-list>. Note
     * that when an item is selected, its associated <os-search-result-row>
     * is not focus()ed at the same time unless it is explicitly clicked/tapped.
     * @private {!mojom.SearchResult}
     */
    selectedItem_: {
      type: Object,
    },

    /**
     * Prevent user deselection by tracking last item selected. This item must
     * only be assigned to an item within |this.$.searchResultList|, and not
     * directly to |this.selectedItem_| or an item within |this.searchResults_|.
     * @private {!mojom.SearchResult}
     */
    lastSelectedItem_: {
      type: Object,
    },

    /**
     * Passed into <iron-list>. Exactly one result is the selectedItem_.
     * @private {!Array<!mojom.SearchResult>}
     */
    searchResults_: {
      type: Array,
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
   * @private {?mojom.SearchHandlerInterface}
   */
  searchHandler_: null,

  listeners: {
    'blur': 'onBlur_',
    'keydown': 'onKeyDown_',
    'search-changed': 'fetchSearchResults_',
  },

  /* override */
  created() {
    this.searchHandler_ = mojom.SearchHandler.getRemote();
  },

  /** @private */
  attached() {
    const toolbarSearchField = this.$.search;
    const searchInput = toolbarSearchField.getSearchInput();
    searchInput.addEventListener(
        'focus', this.onSearchInputFocused_.bind(this));
  },

  /**
   * @param {?mojom.SearchHandlerInterface} searchHandler
   */
  setSearchHandlerForTesting(searchHandler) {
    this.searchHandler_ = searchHandler;
  },

  /**
   * @return {!OsSearchResultRowElement} The <os-search-result-row> that is
   *     associated with the selectedItem.
   * @private
   */
  getSelectedOsSearchResultRow_() {
    return assert(
        this.$.searchResultList.querySelector('os-search-result-row[selected]'),
        'No OsSearchResultRow is selected.');
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
      // TODO(crbug/1056909): Remove once Settings Search is complete.
      fakeSettingsSearchHandlerSearch(query).then(results => {
        this.onSearchResultsReceived_(query, results);
      });
      return;
    }

    // The C++ layer uses base::string16, which use 16 bit characters. JS
    // strings support either 8 or 16 bit characters, and must be converted to
    // an array of 16 bit character codes that match base::string16.
    const queryMojoString16 = {data: Array.from(query, c => c.charCodeAt())};
    this.searchHandler_.search(queryMojoString16).then(response => {
      this.onSearchResultsReceived_(query, response.results);
    });
  },

  /**
   * Updates search results UI when settings search results are fetched.
   * @param {string} query The string used to find search results.
   * @param {!Array<!mojom.SearchResult>} results Array of search results.
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
  onNavigatedtoResultRowRoute_() {
    // Settings has navigated to another page; close search results dropdown.
    this.$.searchResults.close();

    // Blur search input to prevent blinking caret.
    this.$.search.blur();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onBlur_(e) {
    e.stopPropagation();

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
   * @param {!mojom.SearchResult} item The search result item in searchResults_.
   * @return {boolean} True if the item is selected.
   * @private
   */
  isItemSelected_(item) {
    return this.searchResults_.indexOf(item) ===
        this.searchResults_.indexOf(this.selectedItem_);
  },

  /**
   * Returns the correct tab index since <iron-list>'s default tabIndex property
   * does not automatically add selectedItem_'s <os-search-result-row> to the
   * default navigation flow, unless the user explicitly clicks on the row.
   * @param {!mojom.SearchResult} item The search result item in searchResults_.
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

    this.selectedItem_ = this.searchResults_[0];
  },

  /**
   * |selectedItem| is not changed by the time this is called. The value that
   * |selectedItem| will be assigned to is stored in
   * |this.$.searchResultList.selectedItem|.
   * TODO(crbug/1056909): Add test for this specific case.
   * @private
   */
  onSelectedItemChanged_() {
    // <iron-list> causes |this.$.searchResultList.selectedItem| to be null if
    // tapped a second time.
    if (!this.$.searchResultList.selectedItem && this.lastSelectedItem_) {
      // In the case that the user deselects a search result, reselect the item
      // manually by altering the list. Setting |selectedItem| will be no use
      // as |selectedItem| has not been assigned at this point. Adding an
      // observer on |selectedItem| to address a value change will also be no
      // use as it will perpetuate an infinite select and deselect chain in this
      // case.
      this.$.searchResultList.selectItem(this.lastSelectedItem_);
    }
    this.lastSelectedItem_ = this.$.searchResultList.selectedItem;
  },

  /**
   * @param {string} key The string associated with a key.
   * @private
   */
  selectRowViaKeys_(key) {
    assert(key === 'ArrowDown' || key === 'ArrowUp', 'Only arrow keys.');
    assert(!!this.selectedItem_, 'There should be a selected item already.');

    // Select the new item.
    const selectedRowIndex = this.searchResults_.indexOf(this.selectedItem_);
    const numRows = this.searchResults_.length;
    const delta = key === 'ArrowUp' ? -1 : 1;
    const indexOfNewRow = (numRows + selectedRowIndex + delta) % numRows;
    this.selectedItem_ = this.searchResults_[indexOfNewRow];

    if (this.lastFocused_) {
      // If a row was previously focused, focus the currently selected row.
      // Calling focus() on a <os-search-result-row> focuses the element within
      // containing the attribute 'focus-row-control'.
      this.getSelectedOsSearchResultRow_().focus();
    }
  },

  /**
   * Keydown handler to specify how enter-key, arrow-up key, and arrow-down-key
   * interacts with search results in the dropdown. Note that 'Enter' on keyDown
   * when a row is focused is blocked by cr.ui.FocusRowBehavior behavior.
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (!this.shouldShowDropdown_) {
      // No action should be taken if there are no search results.
      return;
    }

    if (e.key === 'Enter') {
      this.getSelectedOsSearchResultRow_().navigateToSearchResultRoute();
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
