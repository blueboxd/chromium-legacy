// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview.Header');

/**
 * @typedef {{numPages: number,
 *            numSheets: number,
 *            pagesLabel: string,
 *            summaryLabel: string}}
 */
print_preview.Header.LabelInfo;

Polymer({
  is: 'print-preview-header',

  behaviors: [SettingsBehavior],

  properties: {
    cloudPrintErrorMessage: String,

    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @type {!print_preview.Error} */
    error: Number,

    firstLoad: Boolean,

    /** @type {!print_preview.State} */
    state: Number,

    /** @private */
    printButtonEnabled_: {
      type: Boolean,
      value: false,
    },

    managed: Boolean,

    /** @private */
    printButtonLabel_: {
      type: String,
      value: function() {
        return loadTimeData.getString('printButton');
      },
    },

    /** @private {?string} */
    summary_: {
      type: String,
      value: null,
    },

    /** @private {?string} */
    summaryLabel_: {
      type: String,
      value: null,
    },
  },

  observers: [
    'update_(settings.copies.value, settings.duplex.value, ' +
        'settings.pages.value, state, destination.id)',
    'updatePrintButtonLabel_(destination.id)'
  ],

  /** @private */
  onPrintClick_: function() {
    this.fire('print-requested');
  },

  /** @private */
  onCancelClick_: function() {
    this.fire('cancel-requested');
  },

  /**
   * @return {boolean}
   * @private
   */
  isPdfOrDrive_: function() {
    return this.destination &&
        (this.destination.id ==
             print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ||
         this.destination.id ==
             print_preview.Destination.GooglePromotedId.DOCS);
  },

  /** @private */
  updatePrintButtonLabel_: function() {
    this.printButtonLabel_ = loadTimeData.getString(
        this.isPdfOrDrive_() ? 'saveButton' : 'printButton');
  },

  /**
   * @return {!print_preview.Header.LabelInfo}
   * @private
   */
  computeLabelInfo_: function() {
    const saveToPdfOrDrive = this.isPdfOrDrive_();
    let numPages = this.getSettingValue('pages').length;
    let numSheets = numPages;
    if (!saveToPdfOrDrive && this.getSettingValue('duplex')) {
      numSheets = Math.ceil(numPages / 2);
    }

    const copies = parseInt(this.getSettingValue('copies'), 10);
    numSheets *= copies;
    numPages *= copies;

    const pagesLabel = loadTimeData.getString('printPreviewPageLabelPlural');
    let summaryLabel;
    if (numSheets > 1) {
      summaryLabel = saveToPdfOrDrive ?
          pagesLabel :
          loadTimeData.getString('printPreviewSheetsLabelPlural');
    } else {
      summaryLabel = loadTimeData.getString(
          saveToPdfOrDrive ? 'printPreviewPageLabelSingular' :
                             'printPreviewSheetsLabelSingular');
    }
    return {
      numPages: numPages,
      numSheets: numSheets,
      pagesLabel: pagesLabel,
      summaryLabel: summaryLabel
    };
  },

  /** @private */
  update_: function() {
    switch (this.state) {
      case (print_preview.State.PRINTING):
        this.printButtonEnabled_ = false;
        this.summary_ = loadTimeData.getString(
            this.isPdfOrDrive_() ? 'saving' : 'printing');
        this.summaryLabel_ = this.summary_;
        break;
      case (print_preview.State.READY):
        this.printButtonEnabled_ = true;
        const labelInfo = this.computeLabelInfo_();
        this.summary_ = this.getSummary_(labelInfo);
        this.summaryLabel_ = this.getSummaryLabel_(labelInfo);
        if (this.firstLoad) {
          this.$$('cr-button.action-button').focus();
          this.fire('print-button-focused');
        }
        break;
      case (print_preview.State.FATAL_ERROR):
        this.summary_ = this.getErrorMessage_();
        this.summaryLabel_ = this.getErrorMessage_();
        this.printButtonEnabled_ = false;
        break;
      default:
        this.summary_ = null;
        this.summaryLabel_ = null;
        this.printButtonEnabled_ = false;
        break;
    }
  },

  /**
   * @return {string} The error message to display.
   * @private
   */
  getErrorMessage_: function() {
    switch (this.error) {
      case print_preview.Error.PRINT_FAILED:
        return loadTimeData.getString('couldNotPrint');
      case print_preview.Error.CLOUD_PRINT_ERROR:
        return this.cloudPrintErrorMessage;
      default:
        return '';
    }
  },

  /**
   * @param {!print_preview.Header.LabelInfo} labelInfo
   * @return {string}
   * @private
   */
  getSummary_: function(labelInfo) {
    if (labelInfo.numSheets === 0) {
      return '';
    }

    let html = loadTimeData.getStringF(
        'printPreviewSummaryFormatShort',
        '<b>' + labelInfo.numSheets.toLocaleString() + '</b>',
        '<b>' + labelInfo.summaryLabel + '</b>');

    // Removing extra spaces from within the string.
    html = html.replace(/\s{2,}/g, ' ');
    return html;
  },

  /**
   * @param {!print_preview.Header.LabelInfo} labelInfo
   * @return {string}
   * @private
   */
  getSummaryLabel_: function(labelInfo) {
    return loadTimeData.getStringF(
        'printPreviewSummaryFormatShort', labelInfo.numSheets.toLocaleString(),
        labelInfo.summaryLabel);
  },
});
