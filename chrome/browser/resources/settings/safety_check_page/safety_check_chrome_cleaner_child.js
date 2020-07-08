// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-passwords-child' is the settings page containing the
 * safety check child showing the password status.
 */
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

import {SafetyCheckCallbackConstants, SafetyCheckChromeCleanerStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';

/**
 * @typedef {{
 *   newState: SafetyCheckChromeCleanerStatus,
 *   displayString: string,
 * }}
 */
let ChromeCleanerChangedEvent;

Polymer({
  is: 'settings-safety-check-chrome-cleaner-child',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check Chrome cleaner child.
     * @private {!SafetyCheckChromeCleanerStatus}
     */
    status_: {
      type: Number,
      value: SafetyCheckChromeCleanerStatus.CHECKING,
    },

    /**
     * UI string to display for this child, received from the backend.
     * @private
     */
    displayString_: String,
  },

  /** @private {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.CHROME_CLEANER_CHANGED,
        this.onSafetyCheckChromeCleanerChanged_.bind(this));
  },

  /**
   * @param {!ChromeCleanerChangedEvent} event
   * @private
   */
  onSafetyCheckChromeCleanerChanged_: function(event) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  },

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_: function() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.CHECKING:
        return SafetyCheckIconStatus.RUNNING;
      case SafetyCheckChromeCleanerStatus.INITIAL:
      case SafetyCheckChromeCleanerStatus.REPORTER_FOUND_NOTHING:
      case SafetyCheckChromeCleanerStatus.SCANNING_FOUND_NOTHING:
      case SafetyCheckChromeCleanerStatus.CLEANING_SUCCEEDED:
      case SafetyCheckChromeCleanerStatus.REPORTER_RUNNING:
      case SafetyCheckChromeCleanerStatus.SCANNING:
        return SafetyCheckIconStatus.SAFE;
      case SafetyCheckChromeCleanerStatus.REPORTER_FAILED:
      case SafetyCheckChromeCleanerStatus.SCANNING_FAILED:
      case SafetyCheckChromeCleanerStatus.CLEANING_FAILED:
      case SafetyCheckChromeCleanerStatus.CLEANER_DOWNLOAD_FAILED:
      case SafetyCheckChromeCleanerStatus.CLEANING:
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
      case SafetyCheckChromeCleanerStatus.DISABLED_BY_ADMIN:
        return SafetyCheckIconStatus.INFO;
      case SafetyCheckChromeCleanerStatus.CONNECTION_LOST:
      case SafetyCheckChromeCleanerStatus.USER_DECLINED_CLEANUP:
      case SafetyCheckChromeCleanerStatus.INFECTED:
        return SafetyCheckIconStatus.WARNING;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getButtonLabel_: function() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.REPORTER_FAILED:
      case SafetyCheckChromeCleanerStatus.SCANNING_FAILED:
      case SafetyCheckChromeCleanerStatus.CLEANING_FAILED:
      case SafetyCheckChromeCleanerStatus.CLEANER_DOWNLOAD_FAILED:
        return this.i18n('passwordViewDetails');
      case SafetyCheckChromeCleanerStatus.CONNECTION_LOST:
      case SafetyCheckChromeCleanerStatus.USER_DECLINED_CLEANUP:
      case SafetyCheckChromeCleanerStatus.INFECTED:
      case SafetyCheckChromeCleanerStatus.CLEANING:
        return this.i18n('safetyCheckReview');
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
        return this.i18n('chromeCleanupRestartButtonLabel');
      default:
        return null;
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getButtonAriaLabel_: function() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.REPORTER_FAILED:
      case SafetyCheckChromeCleanerStatus.SCANNING_FAILED:
      case SafetyCheckChromeCleanerStatus.CLEANING_FAILED:
      case SafetyCheckChromeCleanerStatus.CLEANER_DOWNLOAD_FAILED:
        return this.i18n('safetyCheckReviewErrorDetails');
      case SafetyCheckChromeCleanerStatus.CONNECTION_LOST:
      case SafetyCheckChromeCleanerStatus.USER_DECLINED_CLEANUP:
      case SafetyCheckChromeCleanerStatus.INFECTED:
      case SafetyCheckChromeCleanerStatus.CLEANING:
        return this.i18n('safetyCheckChromeCleanerButtonAriaLabel');
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
        return this.i18n('chromeCleanupRestartButtonLabel');
      default:
        return null;
    }
  },

  /**
   * @private
   * @return {string}
   */
  getButtonClass_: function() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.CONNECTION_LOST:
      case SafetyCheckChromeCleanerStatus.USER_DECLINED_CLEANUP:
      case SafetyCheckChromeCleanerStatus.INFECTED:
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
        return 'action-button';
      default:
        return '';
    }
  },

  /** @private */
  onButtonClick_: function() {
    // TODO(crbug.com/1087263): Add metrics for safety check CCT child user
    // actions.
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.REPORTER_FAILED:
      case SafetyCheckChromeCleanerStatus.SCANNING_FAILED:
      case SafetyCheckChromeCleanerStatus.CONNECTION_LOST:
      case SafetyCheckChromeCleanerStatus.USER_DECLINED_CLEANUP:
      case SafetyCheckChromeCleanerStatus.CLEANING_FAILED:
      case SafetyCheckChromeCleanerStatus.CLEANER_DOWNLOAD_FAILED:
      case SafetyCheckChromeCleanerStatus.INFECTED:
      case SafetyCheckChromeCleanerStatus.CLEANING:
        Router.getInstance().navigateTo(
            routes.CHROME_CLEANUP,
            /* dynamicParams= */ null, /* removeSearch= */ true);
        break;
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
        // TODO(crbug.com/1087263): Implement CCT-based reboot here.
        break;
      default:
        // This is a state without an action.
        break;
    }
  },
});
