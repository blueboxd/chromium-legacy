// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './print_job_entry.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getMetadataProvider} from './mojo_interface_provider.js';

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
    },
  },

  /** @override */
  created() {
    this.mojoInterfaceProvider_ = getMetadataProvider();
  },

  /** @override */
  ready() {
    // TODO(jimmyxgong): Remove this once the app has more capabilities.
    this.$$('#header').textContent = 'Print Management';
    this.mojoInterfaceProvider_.getPrintJobs()
        .then(this.onPrintJobsReceived_.bind(this));
  },

  /**
   * @param {!PrintJobsList} jobs
   * @private
   */
  onPrintJobsReceived_(jobs) {
    // Sort the printers in descending order based on time the print job was
    // created.
    this.printJobs_ = jobs.printJobs.sort((first, second) => {
      return Number(second.creationTime.internalValue) -
          Number(first.creationTime.internalValue);
    });
  }
});
