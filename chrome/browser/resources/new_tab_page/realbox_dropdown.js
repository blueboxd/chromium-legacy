// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './realbox_button.js';
import './realbox_match.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {decodeString16, skColorToRgba} from './utils.js';

/**
 * Indicates a missing suggestion group Id. Based on
 * SearchSuggestionParser::kNoSuggestionGroupId.
 * @type {string}
 */
export const NO_SUGGESTION_GROUP_ID = '-1';

// A dropdown element that contains autocomplete matches. Provides an API for
// the embedder (i.e., <ntp-realbox>) to change the selection.
class RealboxDropdownElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-dropdown';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * @type {!search.mojom.AutocompleteResult}
       */
      result: {
        type: Object,
        observer: 'onResultChange_',
      },

      /**
       * Index of the selected match.
       * @type {number}
       */
      selectedMatchIndex: {
        type: Number,
        value: -1,
        notify: true,
      },

      /**
       * @type {!newTabPage.mojom.SearchBoxTheme}
       */
      theme: {
        type: Object,
        observer: 'onThemeChange_',
      },

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * The list of suggestion group IDs matches belong to.
       * @type {!Array<string>}
       * @private
       */
      groupIds_: {
        type: Array,
        computed: `computeGroupIds_(result)`,
      },

      /**
       * The list of suggestion group IDs whose matches should be hidden.
       * @type {!Array<string>}
       * @private
       */
      hiddenGroupIds_: {
        type: Array,
        computed: `computeHiddenGroupIds_(result)`,
      },

      /**
       * The list of selectable match elements.
       * @type {!Array<!Element>}
       * @private
       */
      selectableMatchElements_: {
        type: Array,
        value: () => [],
      },
    };
  }

  constructor() {
    super();
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
  }

  //============================================================================
  // Public methods
  //============================================================================

  /**
   * Unselects the currently selected match, if any.
   */
  unselect() {
    this.selectedMatchIndex = -1;
  }

  /**
   * Focuses the selected match, if any.
   */
  focusSelected() {
    if (this.$.selector.selectedItem) {
      this.$.selector.selectedItem.focus();
    }
  }

  /**
   * Selects the first match.
   */
  selectFirst() {
    this.selectedMatchIndex = 0;
  }

  /**
   * Selects the match at the given index.
   * @param {number} index
   */
  selectIndex(index) {
    this.selectedMatchIndex = index;
  }

  /**
   * Selects the previous match with respect to the currently selected one.
   * Selects the last match if the first one is currently selected.
   */
  selectPrevious() {
    this.selectedMatchIndex = this.selectedMatchIndex - 1 >= 0 ?
        this.selectedMatchIndex - 1 :
        this.selectableMatchElements_.length - 1;
  }

  /**
   * Selects the last match.
   */
  selectLast() {
    this.selectedMatchIndex = this.selectableMatchElements_.length - 1;
  }

  /**
   * Selects the next match with respect to the currently selected one.
   * Selects the first match if the last one is currently selected.
   */
  selectNext() {
    this.selectedMatchIndex =
        this.selectedMatchIndex + 1 < this.selectableMatchElements_.length ?
        this.selectedMatchIndex + 1 :
        0;
  }

  //============================================================================
  // Callbacks
  //============================================================================

  /**
   * @private
   */
  onResultChange_() {
    // TODO(crbug.com/1041129): Find a more accurate estimate of when the
    // results are actually painted.
    this.dispatchEvent(new CustomEvent('result-repaint', {
      bubbles: true,
      composed: true,
      detail: window.performance.now(),
    }));
  }

  /**
   * @private
   */
  onThemeChange_() {
    if (!loadTimeData.getBoolean('realboxMatchOmniboxTheme')) {
      return;
    }

    const icon = assert(this.theme.icon);
    // Icon's background color in a hovered context (0x29 == .16).
    // TODO(crbug.com/1041129): Share this with the Omnibox.
    const iconBgHovered = {value: icon.value & 0x29ffffff};

    const iconSelected = assert(this.theme.iconSelected);
    // Icon's background color in a focused context (0x52 == .32).
    // TODO(crbug.com/1041129): Share this with the Omnibox.
    const iconBgFocused = {value: iconSelected.value & 0x52ffffff};

    this.updateStyles({
      '--search-box-icon-bg-focused': skColorToRgba(iconBgFocused),
      '--search-box-icon-bg-hovered': skColorToRgba(iconBgHovered),
      '--search-box-icon-selected': skColorToRgba(iconSelected),
      '--search-box-icon': skColorToRgba(icon),
      '--search-box-results-bg-hovered':
          skColorToRgba(assert(this.theme.resultsBgHovered)),
      '--search-box-results-bg-selected':
          skColorToRgba(assert(this.theme.resultsBgSelected)),
      '--search-box-results-bg': skColorToRgba(assert(this.theme.resultsBg)),
      '--search-box-results-dim-selected':
          skColorToRgba(assert(this.theme.resultsDimSelected)),
      '--search-box-results-dim': skColorToRgba(assert(this.theme.resultsDim)),
      '--search-box-results-text-selected':
          skColorToRgba(assert(this.theme.resultsTextSelected)),
      '--search-box-results-text':
          skColorToRgba(assert(this.theme.resultsText)),
      '--search-box-results-url-selected':
          skColorToRgba(assert(this.theme.resultsUrlSelected)),
      '--search-box-results-url': skColorToRgba(assert(this.theme.resultsUrl)),
    });
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @private
   */
  onHeaderFocusin_() {
    // The header got focus. Unselect the selected match, if any.
    this.unselect();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onToggleButtonClick_(e) {
    const groupId = e.target.dataset.id;

    // Tell the backend to toggle visibility of the given suggestion group ID.
    this.pageHandler_.toggleSuggestionGroupIdVisibility(groupId);

    // Hide/Show matches with the given suggestion group ID.
    const index = this.hiddenGroupIds_.indexOf(groupId);
    if (index === -1) {
      this.push('hiddenGroupIds_', groupId);
    } else {
      this.splice('hiddenGroupIds_', index, 1);
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onToggleButtonKeydown_(e) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }

    // Simulate a click so that it gets handled by |onToggleButtonClick_|.
    e.target.click();
    e.preventDefault();  // Prevents default browser action.
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * @returns {number} Index of the match in the autocomplete result. Passed to
   *     the match so it knows abut its position in the list of matches.
   * @private
   */
  matchIndex_(match) {
    if (!this.result || !this.result.matches) {
      return -1;
    }

    return this.result.matches.indexOf(match);
  }

  /**
   * @returns {!Array<string>}
   * @private
   */
  computeGroupIds_() {
    if (!this.result) {
      return [];
    }

    // Add |NO_SUGGESTION_GROUP_ID| to the list of suggestion group IDs.
    return [NO_SUGGESTION_GROUP_ID].concat(
        Object.keys(this.result.suggestionGroupsMap));
  }

  /**
   * @returns {!Array<string>}
   * @private
   */
  computeHiddenGroupIds_() {
    if (!this.result) {
      return [];
    }

    return Object.keys(this.result.suggestionGroupsMap)
        .filter((groupId => {
                  return this.result.suggestionGroupsMap[groupId].hidden;
                }).bind(this));
  }

  /**
   * @param {string} groupId
   * @returns {!function(!search.mojom.AutocompleteMatch):boolean} The filter
   *     function to filter matches that belong to the given suggestion group
   *     ID.
   * @private
   */
  computeMatchBelongsToGroup_(groupId) {
    return (match) => {
      return match.suggestionGroupId === Number(groupId);
    };
  }

  /**
   * @param {string} groupId
   * @returns {boolean} Whether the given suggestion group ID has a header.
   * @private
   */
  groupHasHeader_(groupId) {
    return groupId !== NO_SUGGESTION_GROUP_ID;
  }

  /**
   * @param {string} groupId
   * @returns {boolean} Whether matches with the given suggestion group ID
   *     should be hidden.
   * @private
   */
  groupIsHidden_(groupId) {
    return this.hiddenGroupIds_.indexOf(groupId) !== -1;
  }

  /**
   * @param {string} groupId
   * @returns {string} The header for the given suggestion group ID.
   * @private
   * @suppress {checkTypes}
   */
  headerForGroup_(groupId) {
    if (!this.groupHasHeader_(groupId)) {
      return '';
    }
    return (this.result && this.result.suggestionGroupsMap &&
            this.result.suggestionGroupsMap[groupId]) ?
        decodeString16(this.result.suggestionGroupsMap[groupId].header) :
        '';
  }

  /**
   * @param {string} groupId
   * @returns {string} Tooltip for suggestion group show/hide toggle button.
   * @private
   */
  toggleButtonTitleForGroup_(groupId) {
    if (!this.groupHasHeader_(groupId)) {
      return '';
    }
    return loadTimeData.getString(
        this.groupIsHidden_(groupId) ? 'showSuggestions' : 'hideSuggestions');
  }

  /**
   * @param {string} groupId
   * @returns {string} A11y label for suggestion group show/hide toggle button.
   * @private
   */
  toggleButtonA11yLabelForGroup_(groupId) {
    if (!this.groupHasHeader_(groupId)) {
      return '';
    }
    return loadTimeData.substituteString(
        loadTimeData.getString(
            this.groupIsHidden_(groupId) ? 'showSection' : 'hideSection'),
        this.headerForGroup_(groupId));
  }
}

customElements.define(RealboxDropdownElement.is, RealboxDropdownElement);
