// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './realbox_match.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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

    this.updateStyles({
      '--search-box-results-bg': skColorToRgba(assert(this.theme.resultsBg)),
      '--search-box-results-bg-selected':
          skColorToRgba(assert(this.theme.resultsBgSelected)),
      '--search-box-results-text':
          skColorToRgba(assert(this.theme.resultsText)),
      '--search-box-results-text-selected':
          skColorToRgba(assert(this.theme.resultsTextSelected)),
    });
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @param {!CustomEvent} e
   * @private
   */
  onMatchFocusin_(e) {
    e.stopPropagation();

    this.dispatchEvent(new CustomEvent('match-focusin', {
      bubbles: true,
      composed: true,
      detail: this.selectableMatchElements_.indexOf(
          /** @type {!Element} */ (e.target)),
    }));
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * @returns {!Array<string>}
   * @private
   */
  computeGroupIds_() {
    // Add |NO_SUGGESTION_GROUP_ID| to the list of suggestion group IDs.
    return this.result ? [NO_SUGGESTION_GROUP_ID].concat(
                             Object.keys(this.result.suggestionGroupsMap)) :
                         [];
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
   * @returns {string} The header for the given suggestion group ID.
   * @private
   * @suppress {checkTypes}
   */
  headerForGroup_(groupId) {
    return this.result && this.groupHasHeader_(groupId) ?
        decodeString16(this.result.suggestionGroupsMap[groupId].header) :
        '';
  }
}

customElements.define(RealboxDropdownElement.is, RealboxDropdownElement);
