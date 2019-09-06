// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers-entry-list' is a component for a list
 * of PrinterListEntry's.
 */
Polymer({
  is: 'settings-cups-printers-entry-list',

  behaviors: [
    ListPropertyUpdateBehavior,
  ],

  properties: {
    /**
     * List of printers.
     * @type {!Array<!PrinterListEntry>}
     */
    printers: {
      type: Array,
      value: () => [],
    },

    /**
     * List of printers filtered through a search term.
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    filteredPrinters_: {
      type: Array,
      value: () => [],
    },

    /**
     * Search term for filtering |printers|.
     * @type {string}
     */
    searchTerm: {
      type: String,
      value: '',
    },

    /**
     * Whether to show the no search results message.
     * @type {boolean}
     * @private
     */
     showNoSearchResultsMessage_: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'onSearchChanged_(printers.*, searchTerm)'
  ],

  /**
   * Redoes the search whenever |searchTerm| or |printers| changes.
   * @private
   */
  onSearchChanged_: function() {
    if (!this.printers) {
      return;
    }
    // Filter printers through |searchTerm|. If |searchTerm| is empty,
    // |filteredPrinters_| is just |printers|.
    const updatedPrinters = this.searchTerm ?
        this.printers.filter(
            item =>this.matchesSearchTerm_(item.printerInfo,this.searchTerm)) :
        this.printers.slice();

    updatedPrinters.sort(this.sortPrinters_);

    this.updateList('filteredPrinters_', printer => printer.printerInfo,
        updatedPrinters);

    this.showNoSearchResultsMessage_ =
        !!this.searchTerm && !this.filteredPrinters_.length;
  },


  /**
   * @param {!PrinterListEntry} first
   * @param {!PrinterListEntry} second
   * @return {number}
   * @private
   */
  sortPrinters_: function(first, second) {
    if (first.printerType == second.printerType) {
      return settings.printing.alphabeticalSort(
          first.printerInfo, second.printerInfo);
    }

    // PrinterType sort order maintained in cups_printer_types.js
    return first.printerType - second.printerType;
  },

  /**
   * @param {!CupsPrinterInfo} printer
   * @param {string} searchTerm
   * @return {boolean} True if the printer has |searchTerm| in its name.
   * @private
   */
  matchesSearchTerm_: function(printer, searchTerm) {
    return printer.printerName.toLowerCase().includes(searchTerm.toLowerCase());
  }
});
