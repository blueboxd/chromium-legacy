// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-saved-printers' is a list container for Saved
 * Printers.
 */
Polymer({
  is: 'settings-cups-saved-printers',

  behaviors: [
      WebUIListenerBehavior,
  ],

  properties: {
    /**
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    savedPrinters_: {
      type: Array,
      value: () => [],
    },

    /**
     * Search term for filtering |savedPrinters_|.
     * @type {string}
     */
    searchTerm: {
      type: String,
      value: '',
    },

    /**
     * @type {number}
     * @private
     */
    activePrinterListEntryIndex_: {
      type: Number,
      value: -1,
    },

    /** @type {?CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },
  },

  listeners: {
    'open-action-menu': 'onOpenActionMenu_',
  },

  /** @private {settings.CupsPrintersBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.CupsPrintersBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'on-printers-changed', this.printersChanged_.bind(this));
    this.updateSavedPrintersList();
  },

  /** Public function to update the printer list. */
  updateSavedPrintersList: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrintersList()
        .then(this.printersChanged_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} cupsPrintersList
   * @private
   */
  printersChanged_: function(cupsPrintersList) {
    if (!cupsPrintersList) {
      return;
    }

    this.savedPrinters_ = cupsPrintersList.printerList.map(
        printer => /** @type {!PrinterListEntry} */({
            printerInfo: printer,
            printerType: PrinterType.SAVED}));
  },

  /**
   * @param {!CustomEvent<{target: !HTMLElement, item: !PrinterListEntry}>} e
   * @private
   */
  onOpenActionMenu_: function(e) {
    const item = /** @type {!PrinterListEntry} */(e.detail.item);
    this.activePrinterListEntryIndex_ =
        this.savedPrinters_.findIndex(
            printer => printer.printerInfo == item.printerInfo);
    this.activePrinter =
        this.get(['savedPrinters_', this.activePrinterListEntryIndex_])
        .printerInfo;

    const target = /** @type {!HTMLElement} */ (e.detail.target);
    this.$$('cr-action-menu').showAt(target);
  },

  /** @private */
  onEditTap_: function() {
    // Event is caught by 'settings-cups-printers'.
    this.fire('edit-cups-printer-details');
    this.closeActionMenu_();
  },

  /** @private */
  onRemoveTap_: function() {
    this.splice('savedPrinters_', this.activePrinterListEntryIndex_, 1);
    this.browserProxy_.removeCupsPrinter(
        this.activePrinter.printerId, this.activePrinter.printerName);
    this.activePrinter = null;
    this.activeListEntryIndex_ = -1;
    this.closeActionMenu_();
  },

  /** @private */
  closeActionMenu_: function() {
    this.$$('cr-action-menu').close();
  }
});
