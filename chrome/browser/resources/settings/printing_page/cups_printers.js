// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers' is a component for showing CUPS
 * Printer settings subpage (chrome://settings/cupsPrinters). It is used to
 * set up legacy & non-CloudPrint printers on ChromeOS by leveraging CUPS (the
 * unix printing system) and the many open source drivers built for CUPS.
 */
// TODO(xdai): Rename it to 'settings-cups-printers-page'.
Polymer({
  is: 'settings-cups-printers',

  behaviors: [
      CrNetworkListenerBehavior,
      WebUIListenerBehavior,
  ],

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    printers: {
      type: Array,
      notify: true,
    },

    prefs: Object,

    /** @type {?CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },

    searchTerm: {
      type: String,
    },

    /** @private */
    canAddPrinter_: Boolean,

    /** @private */
    showCupsEditPrinterDialog_: Boolean,

    /**@private */
    addPrinterResultText_: String,

    /**
     * TODO(jimmyxgong): Remove this feature flag conditional once feature
     * is launched.
     * @private
     */
    enableUpdatedUi_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('updatedCupsPrintersUiEnabled');
      },
    },
  },

  listeners: {
    'edit-cups-printer-details': 'onShowCupsEditPrinterDialog_',
    'show-cups-printer-toast': 'openResultToast_',
    'open-manufacturer-model-dialog-for-specified-printer':
        'openManufacturerModelDialogForSpecifiedPrinter_',
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigProxy} */
  networkConfigProxy_: null,

  /** @override */
  created: function() {
    this.networkConfigProxy_ =
        network_config.MojoInterfaceProviderImpl.getInstance()
            .getMojoServiceProxy();
  },

  /** @override */
  attached: function() {
    this.networkConfigProxy_
        .getNetworkStateList({
          filter: chromeos.networkConfig.mojom.FilterType.kActive,
          networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
          limit: chromeos.networkConfig.mojom.kNoLimit,
        })
        .then((responseParams) => {
          this.onActiveNetworksChanged(responseParams.result);
        });

    if (this.enableUpdatedUi_) {
      return;
    }

    this.addWebUIListener(
        'on-printers-changed', this.printersChanged_.bind(this));
  },

  /** @override */
  ready: function() {
    this.updateCupsPrintersList_();
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   * @private
   */
  onActiveNetworksChanged: function(networks) {
    this.canAddPrinter_ = networks.some(function(network) {
      return OncMojo.connectionStateIsConnected(network.connectionState);
    });
  },

  /**
   * @param {!CustomEvent<!{
   *      resultCode: PrinterSetupResult,
   *      printerName: string
   * }>} event
   * @private
   */
   openResultToast_: function(event) {
    const printerName = event.detail.printerName;
    switch (event.detail.resultCode) {
      case PrinterSetupResult.SUCCESS:
        if (this.enableUpdatedUi_) {
          this.$$('#savedPrinters').updateSavedPrintersList();
        } else {
          this.updateCupsPrintersList_();
        }
        this.addPrinterResultText_ =
            loadTimeData.getStringF('printerAddedSuccessfulMessage',
                                    printerName);
        break;
      case PrinterSetupResult.EDIT_SUCCESS:
        if (this.enableUpdatedUi_) {
          this.$$('#savedPrinters').updateSavedPrintersList();
        } else {
          this.updateCupsPrintersList_();
        }
        this.addPrinterResultText_ =
            loadTimeData.getStringF('printerEditedSuccessfulMessage',
                                    printerName);
        break;
      default:
        assertNotReached();
      }

    this.$.errorToast.show();
  },

  /**
   * @param {!CustomEvent<{item: !CupsPrinterInfo}>} e
   * @private
   */
  openManufacturerModelDialogForSpecifiedPrinter_: function(e) {
    const item = e.detail.item;
    this.$.addPrinterDialog
        .openManufacturerModelDialogForSpecifiedPrinter(item);
  },

  /** @private */
  updateCupsPrintersList_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrintersList()
        .then(this.printersChanged_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} cupsPrintersList
   * @private
   */
  printersChanged_: function(cupsPrintersList) {
    this.printers = cupsPrintersList.printerList;
  },

  /** @private */
  onAddPrinterTap_: function() {
    this.$.addPrinterDialog.open();
  },

  /** @private */
  onAddPrinterDialogClose_: function() {
    cr.ui.focusWithoutInk(assert(this.$$('#addPrinter')));
  },

  /** @private */
  onShowCupsEditPrinterDialog_: function() {
    this.showCupsEditPrinterDialog_ = true;
  },

  /** @private */
  onEditPrinterDialogClose_: function() {
    this.showCupsEditPrinterDialog_ = false;
  },

  /**
   * @param {string} searchTerm
   * @return {boolean} If the 'no-search-results-found' string should be shown.
   * @private
   */
  showNoSearchResultsMessage_: function(searchTerm) {
    if (!searchTerm || !this.printers.length) {
      return false;
    }
    searchTerm = searchTerm.toLowerCase();
    return !this.printers.some(printer => {
      return printer.printerName.toLowerCase().includes(searchTerm);
    });
  },

  /**
   * @param {boolean} connectedToNetwork Whether the device is connected to
         a network.
   * @param {boolean} userNativePrintersAllowed Whether users are allowed to
         configure their own native printers.
   * @return {boolean} Whether the 'Add Printer' button is active.
   * @private
   */
  addPrinterButtonActive_: function(
      connectedToNetwork, userNativePrintersAllowed) {
    return connectedToNetwork && userNativePrintersAllowed;
  }
});
