// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-button-strip',

  properties: {
    /** @type {!print_preview.Destination} */
    destination: Object,

    firstLoad: Boolean,

    /** @type {!print_preview.State} */
    state: {
      type: Number,
      observer: 'updatePrintButtonEnabled_',
    },

    /** @private */
    printButtonEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    printButtonLabel_: {
      type: String,
      value: function() {
        return loadTimeData.getString('printButton');
      },
    },
  },

  observers: ['updatePrintButtonLabel_(destination.id)'],

  /** @private {!print_preview.State} */
  lastState_: print_preview.State.NOT_READY,

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

  /** @private */
  updatePrintButtonEnabled_: function() {
    switch (this.state) {
      case (print_preview.State.PRINTING):
        this.printButtonEnabled_ = false;
        break;
      case (print_preview.State.READY):
        this.printButtonEnabled_ = true;
        if (this.firstLoad) {
          this.$$('cr-button.action-button').focus();
          this.fire('print-button-focused');
        }
        break;
      default:
        this.printButtonEnabled_ = false;
        break;
    }
    this.lastState_ = this.state;
  },
});
