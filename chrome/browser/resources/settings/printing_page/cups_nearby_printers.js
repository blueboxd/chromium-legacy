// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-nearby-printers' is a list container for
 * Nearby Printers.
 */
Polymer({
  is: 'settings-cups-nearby-printers',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    nearbyPrinters_: {
      type: Array,
      value: () => [],
    },

    /**
     * Search term for filtering |nearbyPrinters_|.
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
    'add-automatic-printer': 'onAddAutomaticPrinter_',
  },

  /** @override */
  attached: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .startDiscoveringPrinters();
    this.addWebUIListener(
        'on-nearby-printers-changed', this.onNearbyPrintersChanged_.bind(this));
  },

  /**
   * @param {!Array<!CupsPrinterInfo>} automaticPrinters
   * @param {!Array<!CupsPrinterInfo>} discoveredPrinters
   * @private
   */
  onNearbyPrintersChanged_: function(automaticPrinters, discoveredPrinters) {
    if (!automaticPrinters && !discoveredPrinters) {
      return;
    }

    const printers = /** @type{!Array<!PrinterListEntry>} */ ([]);

    for (const printer of automaticPrinters) {
      printers.push({printerInfo: printer,
                     printerType: PrinterType.AUTOMATIC});
    }

    for (const printer of discoveredPrinters) {
      printers.push({printerInfo: printer,
                     printerType: PrinterType.DISCOVERED});
    }

    this.nearbyPrinters_ = printers;
  },

  /**
   * @param {!CustomEvent<{item: !PrinterListEntry}>} e
   * @private
   */
  onAddAutomaticPrinter_: function(e) {
    const item = e.detail.item;
    this.setActivePrinter_(item);

    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .addDiscoveredPrinter(item.printerInfo.printerId)
        .then(this.onAddNearbyPrintersSucceeded_.bind(this,
            item.printerInfo.printerName));
  },

  /**
   * Retrieves the index of |item| in |nearbyPrinters_| and sets that printer as
   * the active printer.
   * @param {!PrinterListEntry} item
   * @private
   */
  setActivePrinter_: function(item) {
    this.activePrinterListEntryIndex_ =
        this.nearbyPrinters_.findIndex(
            printer => printer.printerInfo == item.printerInfo);

    this.activePrinter =
        this.get(['nearbyPrinters_', this.activePrinterListEntryIndex_])
        .printerInfo;
  },

  /**
   * Handler for addDiscoveredPrinter.
   * @param {string} printerName
   * @param {!PrinterSetupResult} result
   * @private
   */
  onAddNearbyPrintersSucceeded_: function(printerName, result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: printerName});
  }
});