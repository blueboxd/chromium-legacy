// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview.HeaderNew');

/**
 * @typedef {{numPages: number,
 *            numSheets: number,
 *            pagesLabel: string,
 *            summaryLabel: string}}
 */
print_preview.HeaderNew.LabelInfo;

Polymer({
  is: 'print-preview-header-new',

  behaviors: [SettingsBehavior],

  properties: {
    cloudPrintErrorMessage: String,

    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @type {!print_preview.Error} */
    error: Number,

    /** @type {!print_preview.State} */
    state: Number,

    managed: Boolean,

    /** @private {?string} */
    summary_: {
      type: String,
      value: null,
    },
  },

  observers: [
    'update_(settings.copies.value, settings.duplex.value, ' +
        'settings.pages.value, state, destination.id)',
  ],

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

  /**
   * @return {!print_preview.HeaderNew.LabelInfo}
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
        this.summary_ = loadTimeData.getString(
            this.isPdfOrDrive_() ? 'saving' : 'printing');
        break;
      case (print_preview.State.READY):
        const labelInfo = this.computeLabelInfo_();
        this.summary_ = this.getSummary_(labelInfo);
        break;
      case (print_preview.State.FATAL_ERROR):
        this.summary_ = this.getErrorMessage_();
        break;
      default:
        this.summary_ = null;
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
   * @param {!print_preview.HeaderNew.LabelInfo} labelInfo
   * @return {string}
   * @private
   */
  getSummary_: function(labelInfo) {
    return labelInfo.numSheets === 0 ?
        '' :
        loadTimeData.getStringF(
            'printPreviewNewSummaryFormatShort',
            labelInfo.numSheets.toLocaleString(), labelInfo.summaryLabel);
  },
});
