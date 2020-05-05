// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './print_job_clear_history_dialog.js';
import './print_job_entry.js';
import './print_management_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getMetadataProvider} from './mojo_interface_provider.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';

/**
 * @typedef {{
 *   printJobs: !Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>
 * }}
 */
let PrintJobsList;

/**
 * @fileoverview
 * 'print-management' is used as the main app to display print jobs.
 */
Polymer({
  is: 'print-management',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {
   *  ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderInterface
   * }
   */
  mojoInterfaceProvider_: null,

  properties: {
    /**
     * @type {!Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
     * @private
     */
    printJobs_: {
      type: Array,
      value: () => [],
    },

    /**
     * Used by FocusRowBehavior to track the last focused element on a row.
     * @private
     */
    lastFocused_: Object,

    /**
     * Used by FocusRowBehavior to track if the list has been blurred.
     * @private
     */
    listBlurred_: Boolean,

    /** @private */
    showClearAllDialog_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'all-history-cleared': 'getPrintJobs_',
  },

  /** @override */
  created() {
    this.mojoInterfaceProvider_ = getMetadataProvider();
  },

  /** @override */
  ready() {
    this.getPrintJobs_();
  },

  /**
   * @param {!PrintJobsList} jobs
   * @private
   */
  onPrintJobsReceived_(jobs) {
    // Sort the printers in descending order based on time the print job was
    // created.
    // TODO(crbug/1073690): Update this when BigInt is supported for
    // updateList().
    this.printJobs_ = jobs.printJobs.sort((first, second) => {
      return Number(second.creationTime.internalValue) -
          Number(first.creationTime.internalValue);
    });
  },

  /** @private */
  getPrintJobs_() {
    this.mojoInterfaceProvider_.getPrintJobs()
        .then(this.onPrintJobsReceived_.bind(this));
  },

  /** @private */
  onClearHistoryClicked_() {
    this.showClearAllDialog_ = true;
  },

  /** @private */
  onClearHistoryDialogClosed_() {
    this.showClearAllDialog_ = false;
  },
});
