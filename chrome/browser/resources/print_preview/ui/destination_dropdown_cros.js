// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
// TODO(gavinwill): Remove iron-dropdown dependency https://crbug.com/1082587.
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

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

    /** @private */
    shouldShowDestinationPrinterStatusIcon_: {
      type: Boolean,
      computed: 'computeShouldShowDestinationPrinterStatusIcon_(value)',
    },
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
    const dropdownInput =
        /** @type {!CrInputElement} */ (this.$$('#dropdownInput'));

    // Exit if path includes |dropdown| because event will be handled by
    // onSelect_.
    if (paths.includes(dropdown)) {
      return;
    }

    if (!paths.includes(dropdownInput) || dropdown.opened) {
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
      case 'ArrowDown': {
        const items = dropdown.getElementsByClassName('list-item');
        if (items.length === 0) {
          break;
        }
        this.updateHighlighted_(event.code === 'ArrowDown');
        break;
      }
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
   * Updates the currently highlighted element based on keyboard up/down
   *    movement.
   * @param {boolean} moveDown
   * @private
   */
  updateHighlighted_(moveDown) {
    const items = this.getButtonListFromDropdown_();
    const numItems = items.length;
    if (numItems === 0) {
      return;
    }

    let nextIndex = 0;
    const currentIndex = this.findHighlightedItemIndex_();
    if (currentIndex === -1) {
      nextIndex = moveDown ? 0 : numItems - 1;
    } else {
      const delta = moveDown ? 1 : -1;
      nextIndex = (numItems + currentIndex + delta) % numItems;
      items[currentIndex].toggleAttribute('highlighted_', false);
    }
    items[nextIndex].toggleAttribute('highlighted_', true);
    // The newly highlighted item might not be visible because the dropdown
    // needs to be scrolled. So scroll the dropdown if necessary.
    items[nextIndex].scrollIntoViewIfNeeded();
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
   * Finds the index of currently highlighted dropdown item.
   * @return {number} Index of the currently highlighted dropdown item, or -1 if
   *   no item is highlighted.
   * @private
   */
  findHighlightedItemIndex_() {
    const items = this.getButtonListFromDropdown_();
    return items.findIndex(item => item.hasAttribute('highlighted_'));
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

  /**
   * Only show the printer status badge if the currently selected destination is
   * a CrOS local printer.
   * @return {boolean}
   * @private
   */
  computeShouldShowDestinationPrinterStatusIcon_: function() {
    return this.value && this.value.origin === DestinationOrigin.CROS;
  },
});
