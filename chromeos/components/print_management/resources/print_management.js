// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
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
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * @typedef {Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
 */
let PrintJobInfoArr;

/**
 * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} first
 * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} second
 * @return {number}
 */
function comparePrintJobsReverseChronologically(first, second) {
  return -comparePrintJobsChronologically(first, second);
}

/**
 * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} first
 * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} second
 * @return {number}
 */
function comparePrintJobsChronologically(first, second) {
  return Number(first.creationTime.internalValue) -
      Number(second.creationTime.internalValue);
}

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

  /**
   * Receiver responsible for observing print job updates notification events.
   * @private {
   *  ?chromeos.printing.printingManager.mojom.PrintJobsObserverReceiver}
   */
  printJobsObserverReceiver_: null,

  properties: {
    /**
     * @type {!PrintJobInfoArr}
     * @private
     */
    printJobs_: {
      type: Array,
      value: () => [],
    },

    /**
     * @type {!PrintJobInfoArr}
     * @private
     */
    ongoingPrintJobs_: {
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
  attached() {
    this.startObservingPrintJobs_();
  },

  /** @override */
  detached() {
    this.printJobsObserverReceiver_.$.close();
  },

  /** @private */
  startObservingPrintJobs_() {
    this.printJobsObserverReceiver_ =
        new chromeos.printing.printingManager.mojom.PrintJobsObserverReceiver
        (
          /**
           * @type {!chromeos.printing.printingManager.mojom.
           *        PrintJobsObserverInterface}
           */
          (this));
    this.mojoInterfaceProvider_.observePrintJobs(
        this.printJobsObserverReceiver_.$.bindNewPipeAndPassRemote())
        .then(() => {
          this.getPrintJobs_();
        });
  },

  /**
   * Overrides chromeos.printing.printingManager.mojom.
   *           PrintJobsObserverInterface
   */
  onAllPrintJobsDeleted() {
    this.getPrintJobs_();
  },

  /**
   * Overrides chromeos.printing.printingManager.mojom.
   *           PrintJobsObserverInterface
   * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} job
   */
  onPrintJobUpdate(job) {
    // Only update ongoing print jobs.
    assert(job.activePrintJobInfo);

    // Check if |job| is an existing ongoing print job and requires an update
    // or if |job| is a new ongoing print job.
    const idx = this.ongoingPrintJobs_.findIndex(
        arr_job => arr_job.id === job.id);
    if (idx !== -1) {
      // Replace the existing ongoing print job with its updated entry.
      this.splice('ongoingPrintJobs_', idx, 1, job);
    } else {
      // New ongoing print jobs are appended to the ongoing print
      // jobs list.
      this.push('ongoingPrintJobs_', job);
    }

    if (job.activePrintJobInfo.activeState ===
        chromeos.printing.printingManager.mojom.ActivePrintJobState
            .kDocumentDone) {
      // This print job is now completed, next step is to update the history
      // list with the recently stored print job.
      this.getPrintJobs_();
    }
  },

  /**
   * @param {!{printJobs: !PrintJobInfoArr}} jobs
   * @private
   */
  onPrintJobsReceived_(jobs) {
    // TODO(crbug/1073690): Update this when BigInt is supported for
    // updateList().
    let ongoingList = [];
    let historyList = [];
    for (const job of jobs.printJobs) {
      // activePrintJobInfo is not null for ongoing print jobs.
      if (job.activePrintJobInfo) {
        ongoingList.push(job);
      }
      else {
        historyList.push(job);
      }
    }

    // Sort the print jobs in chronological order.
    this.ongoingPrintJobs_ =
        ongoingList.sort(comparePrintJobsChronologically);
    this.printJobs_ = historyList.sort(comparePrintJobsReverseChronologically);
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

  /**
   * @return {string}
   * @private
   */
  getHistoryLabel_() {
    return loadTimeData.getString('historyToolTip');
  },
});
