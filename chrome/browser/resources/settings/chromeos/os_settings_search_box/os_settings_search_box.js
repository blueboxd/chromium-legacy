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
Polymer({
  is: 'os-settings-search-box',

  behaviors: [I18nBehavior],

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
      value: [],
      observer: 'onSearchResultsChanged_',
    },

    /** @private */
    shouldShowDropdown_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private */
    searchResultsExist_: {
      type: Boolean,
      value: false,
      computed: 'computeSearchResultsExist_(searchResults_)',
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

  listeners: {
    'blur': 'onBlur_',
    'keydown': 'onKeyDown_',
    'search-changed': 'fetchSearchResults_',
  },

  /** @override */
  attached() {
    const toolbarSearchField = this.$.search;
    const searchInput = toolbarSearchField.getSearchInput();
    searchInput.addEventListener(
        'focus', this.onSearchInputFocused_.bind(this));

    // If the search was initiated by directly entering a search URL, need to
    // sync the URL parameter to the textbox.
    const urlSearchQuery =
        settings.Router.getInstance().getQueryParameters().get('search') || '';

    // Setting the search box value without triggering a 'search-changed'
    // event, to prevent an unnecessary duplicate entry in |window.history|.
    toolbarSearchField.setValue(urlSearchQuery, /*noEvent=*/true);

    // Initialize the announcer once.
    Polymer.IronA11yAnnouncer.requestAvailability();
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
   * @return {string} The current input string.
   * @private
   */
  getCurrentQuery_() {
    return this.$.search.getSearchInput().value;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSearchResultsExist_() {
    return this.searchResults_.length !== 0;
  },

  /** @private */
  fetchSearchResults_() {
    const query = this.getCurrentQuery_();
    if (query === '') {
      this.searchResults_ = [];
      return;
    }

    this.spinnerActive = true;

    // The C++ layer uses base::string16, which use 16 bit characters. JS
    // strings support either 8 or 16 bit characters, and must be converted to
    // an array of 16 bit character codes that match base::string16.
    const queryMojoString16 = {data: Array.from(query, c => c.charCodeAt())};
    settings.getSearchHandler().search(queryMojoString16).then(response => {
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
    if (query !== this.getCurrentQuery_()) {
      // Received search results are invalid as the query has since changed.
      return;
    }

    this.spinnerActive = false;
    this.lastFocused_ = null;
    this.searchResults_ = results;
    settings.recordSearch();
  },

  /** @private */
  onNavigatedToResultRowRoute_() {
    // Settings has navigated to another page; close search results dropdown.
    this.shouldShowDropdown_ = false;

    // Blur search input to prevent blinking caret.
    this.$.search.blur();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onBlur_(e) {
    e.stopPropagation();

    // Close the dropdown because  a region outside the search box was clicked.
    this.shouldShowDropdown_ = false;
  },

  /** @private */
  onSearchInputFocused_() {
    this.lastFocused_ = null;

    if (this.searchResultsExist_) {
      // Restore previous results instead of re-fetching.
      this.shouldShowDropdown_ = true;
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
   * @return {number} Length of the search results array.
   * @private
   */
  getListLength_() {
    return this.searchResults_.length;
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
    return this.isItemSelected_(item) && this.shouldShowDropdown_ ? 0 : -1;
  },

  /** @private */
  onSearchResultsChanged_() {
    // Only show dropdown if focus is on search field with a non empty query.
    this.shouldShowDropdown_ =
        this.$.search.isSearchFocused() && !!this.getCurrentQuery_();

    if (!this.shouldShowDropdown_) {
      return;
    }

    if (!this.searchResultsExist_) {
      this.fire('iron-announce', {text: this.i18n('searchNoResults')});
      return;
    }

    // Select the first search result.
    this.selectedItem_ = this.searchResults_[0];
  },

  /**
   * |selectedItem| is not changed by the time this is called. The value that
   * |selectedItem| will be assigned to is stored in
   * |this.$.searchResultList.selectedItem|.
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

    // The newly selected item might not be visible because the list needs
    // to be scrolled. So scroll the dropdown if necessary.
    this.getSelectedOsSearchResultRow_().scrollIntoViewIfNeeded();
  },

  /**
   * Keydown handler to specify how enter-key, arrow-up key, and arrow-down-key
   * interacts with search results in the dropdown. Note that 'Enter' on keyDown
   * when a row is focused is blocked by cr.ui.FocusRowBehavior behavior.
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (!this.searchResultsExist_ ||
        (!this.$.search.isSearchFocused() && !this.lastFocused_)) {
      // No action should be taken if there are no search results, or when
      // neither the search input nor a <os-search-result-row> is focused
      // (ChromeVox may focus on clear search input button).
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
