// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-add-printer-dialog' includes multiple dialogs to
 * set up a new CUPS printer.
 * Subdialogs include:
 * - 'add-printer-manually-dialog' is a dialog in which user can manually enter
 *   the information to set up a new printer.
 * - 'add-printer-manufacturer-model-dialog' is a dialog in which the user can
 *   manually select the manufacture and model of the new printer.
 */

/**
 * Different dialogs in add printer flow.
 * @enum {string}
 */
const AddPrinterDialogs = {
  MANUALLY: 'add-printer-manually-dialog',
  MANUFACTURER: 'add-printer-manufacturer-model-dialog',
  PRINTSERVER: 'add-print-server-dialog',
};

/**
 * Return a reset CupsPrinterInfo object.
 *  @return {!CupsPrinterInfo}
 */
function getEmptyPrinter_() {
  return {
    ppdManufacturer: '',
    ppdModel: '',
    printerAddress: '',
    printerDescription: '',
    printerId: '',
    printerManufacturer: '',
    printerModel: '',
    printerMakeAndModel: '',
    printerName: '',
    printerPPDPath: '',
    printerPpdReference: {
      userSuppliedPpdUrl: '',
      effectiveMakeAndModel: '',
      autoconf: false,
    },
    printerProtocol: 'ipp',
    printerQueue: 'ipp/print',
    printerStatus: '',
    printServerUri: '',
  };
}

Polymer({
  is: 'add-printer-manually-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {type: Object, notify: true, value: getEmptyPrinter_},

    /** @private */
    addPrinterInProgress_: {
      type: Boolean,
      value: false,
    },

    /**
     * The error text to be displayed on the dialog.
     * @private
     */
    errorText_: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    printServersUiEnabled: Boolean,
  },

  observers: [
    'printerInfoChanged_(newPrinter.*)',
  ],

  /** @private */
  onCancelTap_() {
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter success.
   * @param {!PrinterSetupResult} result
   * @private
   * */
  onAddPrinterSucceeded_(result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: this.newPrinter.printerName});
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter failure.
   * @param {*} result
   * @private
   * */
  onAddPrinterFailed_(result) {
    this.errorText_ = settings.printing.getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  },

  /**
   * Handler for getPrinterInfo success.
   * @param {!PrinterMakeModel} info
   * @private
   * */
  onPrinterFound_(info) {
    const newPrinter =
        /** @type {CupsPrinterInfo}  */ (Object.assign({}, this.newPrinter));

    newPrinter.printerManufacturer = info.manufacturer;
    newPrinter.printerModel = info.model;
    newPrinter.printerMakeAndModel = info.makeAndModel;
    newPrinter.printerPpdReference.userSuppliedPpdUrl =
        info.ppdRefUserSuppliedPpdUrl;
    newPrinter.printerPpdReference.effectiveMakeAndModel =
        info.ppdRefEffectiveMakeAndModel;
    newPrinter.printerPpdReference.autoconf = info.autoconf;

    this.newPrinter = newPrinter;


    // Add the printer if it's configurable. Otherwise, forward to the
    // manufacturer dialog.
    if (info.ppdReferenceResolved) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .addCupsPrinter(this.newPrinter)
          .then(
              this.onAddPrinterSucceeded_.bind(this),
              this.onAddPrinterFailed_.bind(this));
    } else {
      this.$$('add-printer-dialog').close();
      this.fire('open-manufacturer-model-dialog');
    }
  },

  /**
   * Handler for getPrinterInfo failure.
   * @param {*} result a PrinterSetupResult with an error code indicating why
   * getPrinterInfo failed.
   * @private
   */
  infoFailed_(result) {
    this.addPrinterInProgress_ = false;
    if (result == PrinterSetupResult.PRINTER_UNREACHABLE) {
      this.$.printerAddressInput.invalid = true;
      return;
    }
    this.errorText_ = settings.printing.getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  },

  /** @private */
  addPressed_() {
    this.addPrinterInProgress_ = true;

    if (this.newPrinter.printerProtocol == 'ipp' ||
        this.newPrinter.printerProtocol == 'ipps') {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getPrinterInfo(this.newPrinter)
          .then(this.onPrinterFound_.bind(this), this.infoFailed_.bind(this));
    } else {
      this.$$('add-printer-dialog').close();
      this.fire('open-manufacturer-model-dialog');
    }
  },

  /** @private */
  onPrintServerTap_: function() {
    this.$$('add-printer-dialog').close();
    this.fire('open-add-print-server-dialog');
  },

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_(event) {
    this.set('newPrinter.printerProtocol', event.target.value);
  },

  /**
   * @return {boolean} Whether the add printer button is enabled.
   * @private
   */
  canAddPrinter_() {
    return !this.addPrinterInProgress_ &&
        settings.printing.isNameAndAddressValid(this.newPrinter);
  },

  /** @private */
  printerInfoChanged_() {
    this.$.printerAddressInput.invalid = false;
    this.errorText_ = '';
  },

  /**
   * Keypress event handler. If enter is pressed, printer is added if
   * |canAddPrinter_| is true.
   * @param {!Event} event
   * @private
   */
  onKeypress_(event) {
    if (event.key != 'Enter') {
      return;
    }
    event.stopPropagation();

    if (this.canAddPrinter_()) {
      this.addPressed_();
    }
  },

});

Polymer({
  is: 'add-printer-manufacturer-model-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },

    /** @type {?Array<string>} */
    manufacturerList: Array,

    /** @type {?Array<string>} */
    modelList: Array,

    /**
     * Whether the user selected PPD file is valid.
     * @private
     */
    invalidPPD_: {
      type: Boolean,
      value: false,
    },

    /**
     * The base name of a newly selected PPD file.
     * @private
     */
    newUserPPD_: String,

    /**
     * The URL to a printer's EULA.
     * @private
     */
    eulaUrl_: {
      type: String,
      value: '',
    },

    /** @private */
    addPrinterInProgress_: {
      type: Boolean,
      value: false,
    },

    /**
     * The error text to be displayed on the dialog.
     * @private
     */
    errorText_: {
      type: String,
      value: '',
    },
  },

  observers: [
    'selectedManufacturerChanged_(activePrinter.ppdManufacturer)',
    'selectedModelChanged_(activePrinter.ppdModel)',
  ],

  /** @override */
  attached() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterManufacturersList()
        .then(this.manufacturerListChanged_.bind(this));
  },

  close() {
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter success.
   * @param {!PrinterSetupResult} result
   * @private
   * */
  onPrinterAddedSucceeded_(result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: this.activePrinter.printerName});
    this.close();
  },

  /**
   * Handler for addCupsPrinter failure.
   * @param {*} result
   * @private
   * */
  onPrinterAddedFailed_(result) {
    this.addPrinterInProgress_ = false;
    this.errorText_ = settings.printing.getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  },

  /**
   * If the printer is a nearby printer, return make + model with the subtext.
   * Otherwise, return printer name.
   * @return {string} The additional information subtext of the manufacturer and
   * model dialog.
   * @private
   */
  getManufacturerAndModelSubtext_() {
    if (this.activePrinter.printerMakeAndModel) {
      return loadTimeData.getStringF(
          'manufacturerAndModelAdditionalInformation',
          this.activePrinter.printerMakeAndModel);
    }
    return loadTimeData.getStringF(
        'manufacturerAndModelAdditionalInformation',
        this.activePrinter.printerName);
  },

  /**
   * @param {string} manufacturer The manufacturer for which we are retrieving
   *     models.
   * @private
   */
  selectedManufacturerChanged_(manufacturer) {
    // Reset model if manufacturer is changed.
    this.set('activePrinter.ppdModel', '');
    this.modelList = [];
    if (manufacturer && manufacturer.length != 0) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(manufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /**
   * Attempts to get the EULA Url if the selected printer has one.
   * @private
   */
  selectedModelChanged_() {
    this.errorText_ = '';
    if (!this.activePrinter.ppdManufacturer || !this.activePrinter.ppdModel) {
      // Do not check for an EULA unless both |ppdManufacturer| and |ppdModel|
      // are set. Set |eulaUrl_| to be empty in this case.
      this.onGetEulaUrlCompleted_('' /* eulaUrl */);
      return;
    }

    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getEulaUrl(
            this.activePrinter.ppdManufacturer, this.activePrinter.ppdModel)
        .then(this.onGetEulaUrlCompleted_.bind(this));
  },

  /**
   * @param {string} eulaUrl The URL for the printer's EULA.
   * @private
   */
  onGetEulaUrlCompleted_(eulaUrl) {
    this.eulaUrl_ = eulaUrl;
  },

  /**
   * @param {!ManufacturersInfo} manufacturersInfo
   * @private
   */
  manufacturerListChanged_(manufacturersInfo) {
    if (!manufacturersInfo.success) {
      return;
    }
    this.manufacturerList = manufacturersInfo.manufacturers;
    if (this.activePrinter.ppdManufacturer.length != 0) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(this.activePrinter.ppdManufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /**
   * @param {!ModelsInfo} modelsInfo
   * @private
   */
  modelListChanged_(modelsInfo) {
    if (modelsInfo.success) {
      this.modelList = modelsInfo.models;
    }
  },

  /** @private */
  onBrowseFile_() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterPPDPath()
        .then(this.printerPPDPathChanged_.bind(this));
  },

  /**
   * @param {string} path The full path to the selected PPD file
   * @private
   */
  printerPPDPathChanged_(path) {
    this.set('activePrinter.printerPPDPath', path);
    this.invalidPPD_ = !path;
    this.newUserPPD_ = settings.printing.getBaseName(path);
  },

  /** @private */
  onCancelTap_() {
    this.close();
    settings.CupsPrintersBrowserProxyImpl.getInstance().cancelPrinterSetUp(
        this.activePrinter);
  },

  /** @private */
  addPrinter_() {
    this.addPrinterInProgress_ = true;
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .addCupsPrinter(this.activePrinter)
        .then(
            this.onPrinterAddedSucceeded_.bind(this),
            this.onPrinterAddedFailed_.bind(this));
  },

  /**
   * @param {string} ppdManufacturer
   * @param {string} ppdModel
   * @param {string} printerPPDPath
   * @return {boolean} Whether we have enough information to set up the printer
   * @private
   */
  canAddPrinter_(ppdManufacturer, ppdModel, printerPPDPath) {
    return !this.addPrinterInProgress_ &&
        settings.printing.isPPDInfoValid(
            ppdManufacturer, ppdModel, printerPPDPath);
  },
});

Polymer({
  is: 'add-print-server-dialog',

  properties: {
    /** @private {string} */
    printServerAddress_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    errorText_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    inProgress_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /** @private */
  onAddPrintServerTap_: function() {
    this.inProgress_ = true;
    this.$$('#printServerAddressInput').invalid = false;
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .queryPrintServer(this.printServerAddress_)
        .then(
            this.onPrintServerAddedSucceeded_.bind(this),
            this.onPrintServerAddedFailed_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} printers
   * @private
   */
  onPrintServerAddedSucceeded_: function(printers) {
    this.inProgress_ = false;
    this.fire('add-print-server-and-show-toast', {printers: printers});
    this.$$('add-printer-dialog').close();
  },

  /**
   * @param {*} addPrintServerError
   * @private
   */
  onPrintServerAddedFailed_: function(addPrintServerError) {
    this.inProgress_ = false;
    if (addPrintServerError == PrintServerResult.INCORRECT_URL) {
      this.$$('#printServerAddressInput').invalid = true;
      return;
    }
    this.errorText_ = settings.printing.getPrintServerErrorText(
        /** @type {PrintServerResult} */ (addPrintServerError));
  },

  /**
   * Keypress event handler. If enter is pressed, trigger the add event.
   * @param {!Event} event
   * @private
   */
  onKeypress_: function(event) {
    if (event.key != 'Enter') {
      return;
    }
    event.stopPropagation();

    this.onAddPrintServerTap_();
  },
});

Polymer({
  is: 'settings-cups-add-printer-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {
      type: Object,
    },

    /** @private {string} */
    previousDialog_: String,

    /** @private {string} */
    currentDialog_: String,

    /** @private {boolean} */
    showManuallyAddDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showManufacturerDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showAddPrintServerDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    printServersUiEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('consumerPrintServerUiEnabled');
      }
    },
  },

  listeners: {
    'open-manually-add-printer-dialog': 'openManuallyAddPrinterDialog_',
    'open-manufacturer-model-dialog':
        'openManufacturerModelDialogForCurrentPrinter_',
    'open-add-print-server-dialog': 'openPrintServerDialog_',
    'no-detected-printer': 'onNoDetectedPrinter_',
  },

  /** Opens the Add manual printer dialog. */
  open() {
    this.resetData_();
    this.switchDialog_(
        '', AddPrinterDialogs.MANUALLY, 'showManuallyAddDialog_');
  },

  /**
   * Reset all the printer data in the Add printer flow.
   * @private
   */
  resetData_() {
    if (this.newPrinter) {
      this.newPrinter = getEmptyPrinter_();
    }
  },

  /** @private */
  openManuallyAddPrinterDialog_() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUALLY,
        'showManuallyAddDialog_');
  },

  /** @private */
  openManufacturerModelDialogForCurrentPrinter_() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUFACTURER,
        'showManufacturerDialog_');
  },

  /** @param {!CupsPrinterInfo} printer */
  openManufacturerModelDialogForSpecifiedPrinter(printer) {
    this.newPrinter = printer;
    this.switchDialog_(
        '', AddPrinterDialogs.MANUFACTURER, 'showManufacturerDialog_');
  },

  /** @private */
  openPrintServerDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.PRINTSERVER,
        'showAddPrintServerDialog_');
  },

  /**
   * Switch dialog from |fromDialog| to |toDialog|.
   * @param {string} fromDialog
   * @param {string} toDialog
   * @param {string} domIfBooleanName The name of the boolean variable
   *     corresponding to the |toDialog|.
   * @private
   */
  switchDialog_(fromDialog, toDialog, domIfBooleanName) {
    this.previousDialog_ = fromDialog;
    this.currentDialog_ = toDialog;

    this.set(domIfBooleanName, true);
    this.async(function() {
      const dialog = this.$$(toDialog);
      dialog.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
      });
    });
  },
});
