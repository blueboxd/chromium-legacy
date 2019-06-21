// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-searchable-drop-down' implements a search box with a
 * suggestions drop down.
 *
 * If the update-value-on-input flag is set, value will be set to whatever is
 * in the input box. Otherwise, value will only be set when an element in items
 * is clicked.
 */
Polymer({
  is: 'cr-searchable-drop-down',

  properties: {
    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    readonly: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * Whether space should be left below the text field to display an error
     * message. Must be true for |errorMessage| to be displayed.
     */
    errorMessageAllowed: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * When |errorMessage| is set, the text field is highlighted red and
     * |errorMessage| is displayed beneath it.
     */
    errorMessage: String,

    placeholder: String,

    /** @type {!Array<string>} */
    items: {
      type: Array,
      observer: 'onItemsChanged_',
    },

    /** @type {string} */
    value: {
      type: String,
      notify: true,
    },

    /** @private {string} */
    searchTerm_: String,

    /** @type {string} */
    label: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    updateValueOnInput: Boolean,

    /** @private {boolean} */
    dropdownRefitPending_: Boolean,

    /** @private {boolean} */
    showLoading: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Enqueue a task to refit the iron-dropdown if it is open.
   * @suppress {missingProperties} Property refit never defined on dropdown
   * @private
   */
  enqueueDropdownRefit_: function() {
    const dropdown = this.$$('iron-dropdown');
    if (!this.dropdownRefitPending_ && dropdown.opened) {
      this.dropdownRefitPending_ = true;
      setTimeout(() => {
        dropdown.refit();
        this.dropdownRefitPending_ = false;
      }, 0);
    }
  },

  /**
   * @param {!Array<string>} oldValue
   * @param {!Array<string>} newValue
   * @private
   */
  onItemsChanged_: function(oldValue, newValue) {
    // Refit the iron-dropdown so that it can expand as neccessary to
    // accommodate new items. Refitting is done on a new task because the change
    // notification might not yet have propagated to the iron-dropdown.
    this.enqueueDropdownRefit_();
  },

  /** @private */
  onClick_: function() {
    if (!this.readonly) {
      this.$$('iron-dropdown').open();
    }
  },

  /** @private */
  onInput_: function() {
    this.searchTerm_ = this.$.search.value;

    if (this.updateValueOnInput) {
      this.value = this.$.search.value;
    }

    // iron-dropdown sets its max-height when it is opened. If the current value
    // results in no filtered items in the drop down list, the iron-dropdown
    // will have a max-height for 0 items. If the user then clears the input
    // field, a non-zero number of items might be displayed in the drop-down,
    // but the height is still limited based on 0 items. This results in a tiny,
    // but scollable dropdown. Refitting the dropdown allows it to expand to
    // accommodate the new items.
    this.enqueueDropdownRefit_();
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onSelect_: function(event) {
    this.$$('iron-dropdown').close();

    this.value = event.model.item;
    this.searchTerm_ = '';
  },

  /** @private */
  filterItems_: function(searchTerm) {
    if (!searchTerm) {
      return null;
    }
    return function(item) {
      return item.toLowerCase().includes(searchTerm.toLowerCase());
    };
  },

  /**
   * @param {string} errorMessage
   * @param {boolean} errorMessageAllowed
   * @return {boolean}
   * @private
   */
  shouldShowErrorMessage_: function(errorMessage, errorMessageAllowed) {
    return !!this.getErrorMessage_(errorMessage, errorMessageAllowed);
  },

  /**
   * @param {string} errorMessage
   * @param {boolean} errorMessageAllowed
   * @return {string}
   * @private
   */
  getErrorMessage_: function(errorMessage, errorMessageAllowed) {
    if (!errorMessageAllowed) {
      return '';
    }
    return errorMessage;
  },

});
