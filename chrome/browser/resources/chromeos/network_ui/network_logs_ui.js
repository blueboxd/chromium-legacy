// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element for UI controlling the storing of system logs.
 */

Polymer({
  is: 'network-logs-ui',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether to store the system_logs file sent with Feedback reports.
     * @private
     */
    systemLogs_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether to filter PII in the system_logs file.
     * @private
     */
    filterPII_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether to store the zipped debugd log files.
     * @private
     */
    debugLogs_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether to store the chrome logs with the zipped log files.
     * @private
     */
    chromeLogs_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether to store the policies .json file.
     * @private
     */
    policies_: {
      type: Boolean,
      value: false,
    },

    /**
     * Shill debugging level.
     * @private
     */
    shillDebugging_: {
      type: String,
      value: 'unknown',
    }
  },

  observers: ['onShillDebuggingChanged_(shillDebugging_)'],

  /** @type {!network_ui.NetworkUIBrowserProxy} */
  browserProxy_: network_ui.NetworkUIBrowserProxyImpl.getInstance(),

  /** @override */
  attached() {},

  /* @private */
  validOptions_() {
    return this.systemLogs_ || this.policies_ || this.debugLogs_;
  },

  /* @private */
  onShillDebuggingChanged_() {
    const shillDebugging = this.shillDebugging_;
    if (!shillDebugging || shillDebugging == 'unknown')
      return;
    this.browserProxy_.setShillDebugging(shillDebugging).then((response) => {
      /*const result =*/ response.shift();
      const isError = response.shift();
      if (isError) {
        console.error('setShillDebugging: ' + shillDebugging + ' failed.');
      }
    });
  },

  /* @private */
  onStore_() {
    const options = {
      systemLogs: this.systemLogs_,
      filterPII: this.filterPII_,
      debugLogs: this.debugLogs_,
      chromeLogs: this.chromeLogs_,
      policies: this.policies_,
    };
    this.$.storeResult.innerText = this.i18n('networkLogsStatus');
    this.$.storeResult.classList.toggle('error', false);
    this.browserProxy_.storeLogs(options).then((response) => {
      const result = response.shift();
      const isError = response.shift();
      this.$.storeResult.innerText = result;
      this.$.storeResult.classList.toggle('error', isError);
    });
  },
});
