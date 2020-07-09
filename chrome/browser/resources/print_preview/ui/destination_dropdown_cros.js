// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
// TODO(gavinwill): Remove iron-dropdown dependency https://crbug.com/1082587.
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import './print_preview_vars_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, DestinationOrigin} from '../data/destination.js';
import {PrinterStatusReason} from '../data/printer_status_cros.js';

import {PrinterState} from './printer_status_icon_cros.js';

Polymer({
  is: 'print-preview-destination-dropdown-cros',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!Destination} */
    value: Object,

    /** @type {!Array<!Destination>} */
    itemList: {
      type: Array,
      observer: 'enqueueDropdownRefit_',
    },

    /** @type {boolean} */
    disabled: {
      type: Boolean,
      value: false,
    },

    driveDestinationKey: String,

    noDestinations: Boolean,

    pdfPrinterDisabled: Boolean,

    pdfDestinationKey: String,

    destinationIcon: String,

    isCurrentDestinationCrosLocal: Boolean,
  },

  listeners: {
    'mousemove': 'onMouseMove_',
  },

  /** @override */
  attached() {
    this.pointerDownListener_ = event => this.onPointerDown_(event);
    document.addEventListener('pointerdown', this.pointerDownListener_);
  },

  /** @override */
  detached() {
    document.removeEventListener('pointerdown', this.pointerDownListener_);
  },

  /**
   * Enqueues a task to refit the iron-dropdown if it is open.
   * @private
   */
  enqueueDropdownRefit_() {
    const dropdown = this.$$('iron-dropdown');
    if (!this.dropdownRefitPending_ && dropdown.opened) {
      this.dropdownRefitPending_ = true;
      setTimeout(() => {
        dropdown.refit();
        this.dropdownRefitPending_ = false;
      }, 0);
    }
  },

  /** @private */
  openDropdown_() {
    if (this.disabled) {
      return;
    }

    const selectedItem = this.getButtonListFromDropdown_().find(
        item => item.value === this.value.key);
    if (selectedItem) {
      selectedItem.toggleAttribute('highlighted_', true);
    }

    this.$$('iron-dropdown').open();
    this.opened_ = true;
  },

  /** @private */
  closeDropdown_() {
    this.$$('iron-dropdown').close();
    this.opened_ = false;

    const highlightedItem = this.findHighlightedItem_();
    if (highlightedItem) {
      highlightedItem.toggleAttribute('highlighted_', false);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMouseMove_(event) {
    const item = event.composedPath().find(
        elm => elm.classList && elm.classList.contains('list-item'));
    if (!item) {
      return;
    }

    // Highlight the item the mouse is hovering over. If the user uses the
    // keyboard, the highlight will shift. But once the user moves the mouse,
    // the highlight should be updated based on the location of the mouse
    // cursor.
    const highlightedItem = this.findHighlightedItem_();
    if (item === highlightedItem) {
      return;
    }

    if (highlightedItem) {
      highlightedItem.toggleAttribute('highlighted_', false);
    }
    item.toggleAttribute('highlighted_', true);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onPointerDown_(event) {
    const paths = event.composedPath();
    const dropdown =
        /** @type {!IronDropdownElement} */ (this.$$('iron-dropdown'));
    const destinationDropdown =
        /** @type {!Element} */ (this.$$('#destination-dropdown'));

    // Exit if path includes |dropdown| because event will be handled by
    // onSelect_.
    if (paths.includes(dropdown)) {
      return;
    }

    if (!paths.includes(destinationDropdown) || dropdown.opened) {
      this.closeDropdown_();
      return;
    }

    this.openDropdown_();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelect_(event) {
    this.dropdownValueSelected_(/** @type {!Element} */ (event.currentTarget));
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeyDown_(event) {
    event.stopPropagation();
    const dropdown = this.$$('iron-dropdown');
    switch (event.code) {
      case 'Tab':
        this.closeDropdown_();
        break;
      case 'ArrowUp':
      case 'ArrowDown':
        this.onArrowKeyPress_(event.code);
        break;
      case 'Enter': {
        if (dropdown.opened) {
          this.dropdownValueSelected_(this.findHighlightedItem_());
          break;
        }
        this.openDropdown_();
        break;
      }
      case 'Escape': {
        if (dropdown.opened) {
          this.closeDropdown_();
          event.preventDefault();
        }
        break;
      }
    }
  },

  /**
   * @param {string} eventCode
   * @private
   */
  onArrowKeyPress_(eventCode) {
    const dropdown = this.$$('iron-dropdown');
    const items = this.getButtonListFromDropdown_();
    if (items.length === 0) {
      return;
    }

    // If the dropdown is open, use the arrow key press to change which item is
    // highlighted in the dropdown. If the dropdown is closed, use the arrow key
    // press to change the selected destination.
    if (dropdown.opened) {
      const currentIndex =
          items.findIndex(item => item.hasAttribute('highlighted_'));
      const nextIndex =
          this.getNextItemIndexInList_(eventCode, currentIndex, items.length);
      if (nextIndex === -1) {
        return;
      }
      items[currentIndex].toggleAttribute('highlighted_', false);
      items[nextIndex].toggleAttribute('highlighted_', true);
    } else {
      const currentIndex =
          items.findIndex(item => item.value === this.value.key);
      const nextIndex =
          this.getNextItemIndexInList_(eventCode, currentIndex, items.length);
      if (nextIndex === -1) {
        return;
      }
      this.fire('dropdown-value-selected', items[nextIndex]);
    }
  },

  /**
   * @param {string} eventCode
   * @param {number} currentIndex
   * @param {number} numItems
   * @return {number} Returns -1 when the next item would be outside the list.
   * @private
   */
  getNextItemIndexInList_(eventCode, currentIndex, numItems) {
    const nextIndex =
        eventCode === 'ArrowDown' ? currentIndex + 1 : currentIndex - 1;
    return nextIndex >= 0 && nextIndex < numItems ? nextIndex : -1;
  },

  /**
   * @param {Element|undefined} dropdownItem
   * @private
   */
  dropdownValueSelected_(dropdownItem) {
    this.closeDropdown_();
    if (dropdownItem) {
      this.fire('dropdown-value-selected', dropdownItem);
    }
  },

  /**
   * Finds the currently highlighted dropdown item.
   * @return {Element|undefined} Currently highlighted dropdown item, or
   *   undefined if no item is highlighted.
   * @private
   */
  findHighlightedItem_() {
    const items = this.getButtonListFromDropdown_();
    return items.find(item => item.hasAttribute('highlighted_'));
  },

  /**
   * Returns list of all the visible items in the dropdown.
   * @return {!Array<!Element>}
   * @private
   */
  getButtonListFromDropdown_() {
    const dropdown = this.$$('iron-dropdown');
    return Array.from(dropdown.getElementsByClassName('list-item'))
        .filter(item => !item.hidden);
  },

  /**
   * @param {?PrinterStatusReason} printerStatusReason
   * @return {number}
   * @private
   */
  computePrinterState_(printerStatusReason) {
    if (!printerStatusReason ||
        printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
      return PrinterState.UNKNOWN;
    }
    if (printerStatusReason === PrinterStatusReason.NO_ERROR) {
      return PrinterState.GOOD;
    }
    return PrinterState.ERROR;
  },
});
