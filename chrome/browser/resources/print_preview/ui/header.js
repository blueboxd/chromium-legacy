// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './print_preview_vars_css.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {Error, State} from '../data/state.js';
import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-header',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior],

  properties: {
    cloudPrintErrorMessage: String,

    /** @type {!Destination} */
    destination: Object,

    /** @type {!Error} */
    error: Number,

    /** @type {!State} */
    state: Number,

    managed: Boolean,

    /** @private {number} */
    sheetCount: Number,

    /** @private {?string} */
    summary_: {
      type: String,
      computed: 'computeSummary_(sheetCount, state, destination.id)',
    },
  },

  /**
   * @return {boolean}
   * @private
   */
  isPdfOrDrive_() {
    return this.destination &&
        (this.destination.id === Destination.GooglePromotedId.SAVE_AS_PDF ||
         this.destination.id === Destination.GooglePromotedId.DOCS);
  },

  /**
   * @return {?string}
   * @private
   */
  computeSummary_() {
    switch (this.state) {
      case (State.PRINTING):
        return loadTimeData.getString(
            this.isPdfOrDrive_() ? 'saving' : 'printing');
      case (State.READY):
        return this.getSheetsSummary_();
      case (State.FATAL_ERROR):
        return this.getErrorMessage_();
      default:
        return null;
    }
  },

  /**
   * @return {string} The error message to display.
   * @private
   */
  getErrorMessage_() {
    switch (this.error) {
      case Error.PRINT_FAILED:
        return loadTimeData.getString('couldNotPrint');
      case Error.CLOUD_PRINT_ERROR:
        return this.cloudPrintErrorMessage;
      default:
        return '';
    }
  },

  /**
   * @return {string}
   * @private
   */
  getSheetsSummary_() {
    if (this.sheetCount === 0) {
      return '';
    }

    const pageOrSheets = this.isPdfOrDrive_() ? 'Page' : 'Sheets';
    const singularOrPlural = this.sheetCount > 1 ? 'Plural' : 'Singular';
    const label = loadTimeData.getString(
        `printPreview${pageOrSheets}Label${singularOrPlural}`);
    return loadTimeData.getStringF(
        'printPreviewSummaryFormatShort', this.sheetCount.toLocaleString(),
        label);
  },
});
