// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-page' is the settings page containing the browser
 * safety check.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.m.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {flush, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from '../autofill_page/password_manager_proxy.js';
import {HatsBrowserProxyImpl} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {LifetimeBrowserProxy, LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.m.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

import {SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckExtensionsStatus, SafetyCheckParentStatus, SafetyCheckPasswordsStatus, SafetyCheckSafeBrowsingStatus, SafetyCheckUpdatesStatus} from './safety_check_browser_proxy.js';


/**
 * UI states a safety check child can be in. Defines the basic UI of the child.
 * @enum {number}
 */
const ChildUiStatus = {
  RUNNING: 0,
  SAFE: 1,
  INFO: 2,
  WARNING: 3,
};

/**
 * @typedef {{
 *   newState: SafetyCheckParentStatus,
 *   displayString: string,
 * }}
 */
let ParentChangedEvent;

/**
 * @typedef {{
 *   newState: SafetyCheckUpdatesStatus,
 *   displayString: string,
 * }}
 */
let UpdatesChangedEvent;

/**
 * @typedef {{
 *   newState: SafetyCheckPasswordsStatus,
 *   displayString: string,
 * }}
 */
let PasswordsChangedEvent;

/**
 * @typedef {{
 *   newState: SafetyCheckSafeBrowsingStatus,
 *   displayString: string,
 * }}
 */
let SafeBrowsingChangedEvent;

/**
 * @typedef {{
 *   newState: SafetyCheckExtensionsStatus,
 *   displayString: string,
 * }}
 */
let ExtensionsChangedEvent;

Polymer({
  is: 'settings-safety-check-page',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check parent element.
     * @private {!SafetyCheckParentStatus}
     */
    parentStatus_: {
      type: Number,
      value: SafetyCheckParentStatus.BEFORE,
    },

    /**
     * Current state of the safety check updates element.
     * @private {!SafetyCheckUpdatesStatus}
     */
    updatesStatus_: {
      type: Number,
      value: SafetyCheckUpdatesStatus.CHECKING,
    },

    /**
     * Current state of the safety check passwords element.
     * @private {!SafetyCheckPasswordsStatus}
     */
    passwordsStatus_: {
      type: Number,
      value: SafetyCheckPasswordsStatus.CHECKING,
    },

    /**
     * Current state of the safety check safe browsing element.
     * @private {!SafetyCheckSafeBrowsingStatus}
     */
    safeBrowsingStatus_: {
      type: Number,
      value: SafetyCheckSafeBrowsingStatus.CHECKING,
    },

    /**
     * Current state of the safety check extensions element.
     * @private {!SafetyCheckExtensionsStatus}
     */
    extensionsStatus_: {
      type: Number,
      value: SafetyCheckExtensionsStatus.CHECKING,
    },

    /**
     * UI string to display for the parent status.
     * @private
     */
    parentDisplayString_: String,

    /**
     * UI string to display for the updates status.
     * @private
     */
    updatesDisplayString_: String,

    /**
     * UI string to display for the passwords status.
     * @private
     */
    passwordsDisplayString_: String,

    /**
     * UI string to display for the Safe Browsing status.
     * @private
     */
    safeBrowsingDisplayString_: String,

    /**
     * UI string to display for the extensions status.
     * @private
     */
    extensionsDisplayString_: String,
  },

  /** @private {SafetyCheckBrowserProxy} */
  safetyCheckBrowserProxy_: null,

  /** @private {?LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @private {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /**
   * Timer ID for periodic update.
   * @private {number}
   */
  updateTimerId_: -1,

  /** @override */
  attached: function() {
    this.safetyCheckBrowserProxy_ = SafetyCheckBrowserProxyImpl.getInstance();
    this.lifetimeBrowserProxy_ = LifetimeBrowserProxyImpl.getInstance();
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.PARENT_CHANGED,
        this.onSafetyCheckParentChanged_.bind(this));
    this.addWebUIListener(
        SafetyCheckCallbackConstants.UPDATES_CHANGED,
        this.onSafetyCheckUpdatesChanged_.bind(this));
    this.addWebUIListener(
        SafetyCheckCallbackConstants.PASSWORDS_CHANGED,
        this.onSafetyCheckPasswordsChanged_.bind(this));
    this.addWebUIListener(
        SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED,
        this.onSafetyCheckSafeBrowsingChanged_.bind(this));
    this.addWebUIListener(
        SafetyCheckCallbackConstants.EXTENSIONS_CHANGED,
        this.onSafetyCheckExtensionsChanged_.bind(this));

    // Configure default UI.
    this.parentDisplayString_ =
        this.i18n('safetyCheckParentPrimaryLabelBefore');
  },

  /**
   * Triggers the safety check.
   * @private
   */
  runSafetyCheck_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFETY_CHECK_START);
    this.metricsBrowserProxy_.recordAction('Settings.SafetyCheck.Start');

    // Trigger safety check.
    this.safetyCheckBrowserProxy_.runSafetyCheck();
    // Readout new safety check status via accessibility.
    this.fire('iron-announce', {
      text: this.i18n('safetyCheckAriaLiveRunning'),
    });
  },

  /**
   * @param {!ParentChangedEvent} event
   * @private
   */
  onSafetyCheckParentChanged_: function(event) {
    this.parentStatus_ = event.newState;
    this.parentDisplayString_ = event.displayString;
    if (this.parentStatus_ === SafetyCheckParentStatus.AFTER) {
      // Start periodic safety check parent ran string updates.
      const update = async () => {
        this.parentDisplayString_ =
            await this.safetyCheckBrowserProxy_.getParentRanDisplayString();
      };
      clearInterval(this.updateTimerId_);
      this.updateTimerId_ = setInterval(update, 60000);
      // Run initial safety check parent ran string update now.
      update();
      // Readout new safety check status via accessibility.
      this.fire('iron-announce', {
        text: this.i18n('safetyCheckAriaLiveAfter'),
      });
    }
  },

  /**
   * @param {!UpdatesChangedEvent} event
   * @private
   */
  onSafetyCheckUpdatesChanged_: function(event) {
    this.updatesStatus_ = event.newState;
    this.updatesDisplayString_ = event.displayString;
  },

  /**
   * @param {!PasswordsChangedEvent} event
   * @private
   */
  onSafetyCheckPasswordsChanged_: function(event) {
    this.passwordsDisplayString_ = event.displayString;
    this.passwordsStatus_ = event.newState;
  },

  /**
   * @param {!SafeBrowsingChangedEvent} event
   * @private
   */
  onSafetyCheckSafeBrowsingChanged_: function(event) {
    this.safeBrowsingStatus_ = event.newState;
    this.safeBrowsingDisplayString_ = event.displayString;
  },

  /**
   * @param {!ExtensionsChangedEvent} event
   * @private
   */
  onSafetyCheckExtensionsChanged_: function(event) {
    this.extensionsDisplayString_ = event.displayString;
    this.extensionsStatus_ = event.newState;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowParentButton_: function() {
    return this.parentStatus_ === SafetyCheckParentStatus.BEFORE;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowParentIconButton_: function() {
    return this.parentStatus_ !== SafetyCheckParentStatus.BEFORE;
  },

  /** @private */
  onRunSafetyCheckClick_: function() {
    HatsBrowserProxyImpl.getInstance().tryShowSurvey();

    this.runSafetyCheck_();

    // Update parent element so that re-run button is visible and can be
    // focused.
    this.parentStatus_ = SafetyCheckParentStatus.CHECKING;
    flush();
    this.focusParent_();
  },

  /** @private */
  focusParent_() {
    const element =
        /** @type {!Element} */ (this.$$('#safetyCheckParentIconButton'));
    element.focus();
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowChildren_: function() {
    return this.parentStatus_ != SafetyCheckParentStatus.BEFORE;
  },

  /**
   * Returns the default icon for a safety check child in the specified state.
   * @private
   * @param {ChildUiStatus} childUiStatus
   * @return {?string}
   */
  getChildUiIcon_: function(childUiStatus) {
    switch (childUiStatus) {
      case ChildUiStatus.RUNNING:
        return null;
      case ChildUiStatus.SAFE:
        return 'cr:check';
      case ChildUiStatus.INFO:
        return 'cr:info';
      case ChildUiStatus.WARNING:
        return 'cr:warning';
      default:
        assertNotReached();
    }
  },

  /**
   * Returns the default icon src for animated icons for a safety check child
   * in the specified state.
   * @private
   * @param {ChildUiStatus} childUiStatus
   * @return {?string}
   */
  getChildUiIconSrc_: function(childUiStatus) {
    if (childUiStatus === ChildUiStatus.RUNNING) {
      return 'chrome://resources/images/throbber_small.svg';
    }
    return null;
  },

  /**
   * Returns the default icon class for a safety check child in the specified
   * state.
   * @private
   * @param {ChildUiStatus} childUiStatus
   * @return {string}
   */
  getChildUiIconClass_: function(childUiStatus) {
    switch (childUiStatus) {
      case ChildUiStatus.RUNNING:
      case ChildUiStatus.SAFE:
        return 'icon-blue';
      case ChildUiStatus.WARNING:
        return 'icon-red';
      default:
        return '';
    }
  },

  /**
   * Returns the default icon aria label for a safety check child in the
   * specified state.
   * @private
   * @param {ChildUiStatus} childUiStatus
   * @return {string}
   */
  getChildUiIconAriaLabel_: function(childUiStatus) {
    switch (childUiStatus) {
      case ChildUiStatus.RUNNING:
        return this.i18n('safetyCheckIconRunningAriaLabel');
      case ChildUiStatus.SAFE:
        return this.i18n('safetyCheckIconSafeAriaLabel');
      case ChildUiStatus.INFO:
        return this.i18n('safetyCheckIconInfoAriaLabel');
      case ChildUiStatus.WARNING:
        return this.i18n('safetyCheckIconWarningAriaLabel');
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowUpdatesButton_: function() {
    return this.updatesStatus_ === SafetyCheckUpdatesStatus.RELAUNCH;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowUpdatesManagedIcon_: function() {
    return this.updatesStatus_ === SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN;
  },

  /** @private */
  onSafetyCheckUpdatesButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFETY_CHECK_UPDATES_RELAUNCH);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.RelaunchAfterUpdates');

    this.lifetimeBrowserProxy_.relaunch();
  },

  /**
   * @return {ChildUiStatus}
   * @private
   */
  getUpdatesUiStatus_: function() {
    switch (this.updatesStatus_) {
      case SafetyCheckUpdatesStatus.CHECKING:
      case SafetyCheckUpdatesStatus.UPDATING:
        return ChildUiStatus.RUNNING;
      case SafetyCheckUpdatesStatus.UPDATED:
        return ChildUiStatus.SAFE;
      case SafetyCheckUpdatesStatus.RELAUNCH:
      case SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
      case SafetyCheckUpdatesStatus.FAILED_OFFLINE:
      case SafetyCheckUpdatesStatus.UNKNOWN:
        return ChildUiStatus.INFO;
      case SafetyCheckUpdatesStatus.FAILED:
        return ChildUiStatus.WARNING;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getUpdatesIcon_: function() {
    return this.getChildUiIcon_(this.getUpdatesUiStatus_());
  },

  /**
   * @private
   * @return {?string}
   */
  getUpdatesIconSrc_: function() {
    return this.getChildUiIconSrc_(this.getUpdatesUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getUpdatesIconClass_: function() {
    return this.getChildUiIconClass_(this.getUpdatesUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getUpdatesIconAriaLabel_: function() {
    return this.getChildUiIconAriaLabel_(this.getUpdatesUiStatus_());
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowPasswordsButton_: function() {
    return this.passwordsStatus_ === SafetyCheckPasswordsStatus.COMPROMISED;
  },

  /**
   * @private
   * @return {ChildUiStatus}
   */
  getPasswordsUiStatus_: function() {
    switch (this.passwordsStatus_) {
      case SafetyCheckPasswordsStatus.CHECKING:
        return ChildUiStatus.RUNNING;
      case SafetyCheckPasswordsStatus.SAFE:
        return ChildUiStatus.SAFE;
      case SafetyCheckPasswordsStatus.COMPROMISED:
        return ChildUiStatus.WARNING;
      case SafetyCheckPasswordsStatus.OFFLINE:
      case SafetyCheckPasswordsStatus.NO_PASSWORDS:
      case SafetyCheckPasswordsStatus.SIGNED_OUT:
      case SafetyCheckPasswordsStatus.QUOTA_LIMIT:
      case SafetyCheckPasswordsStatus.ERROR:
        return ChildUiStatus.INFO;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getPasswordsIcon_: function() {
    return this.getChildUiIcon_(this.getPasswordsUiStatus_());
  },

  /**
   * @private
   * @return {?string}
   */
  getPasswordsIconSrc_: function() {
    return this.getChildUiIconSrc_(this.getPasswordsUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getPasswordsIconClass_: function() {
    return this.getChildUiIconClass_(this.getPasswordsUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getPasswordsIconAriaLabel_: function() {
    return this.getChildUiIconAriaLabel_(this.getPasswordsUiStatus_());
  },

  /** @private */
  onPasswordsButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFETY_CHECK_PASSWORDS_MANAGE);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ManagePasswords');

    Router.getInstance().navigateTo(routes.CHECK_PASSWORDS);
    PasswordManagerImpl.getInstance().recordPasswordCheckReferrer(
        PasswordManagerProxy.PasswordCheckReferrer.SAFETY_CHECK);
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowSafeBrowsingButton_: function() {
    return this.safeBrowsingStatus_ === SafetyCheckSafeBrowsingStatus.DISABLED;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowSafeBrowsingManagedIcon_: function() {
    return this.getSafeBrowsingManagedIcon_() != null;
  },

  /**
   * @private
   * @return {?string}
   */
  getSafeBrowsingManagedIcon_: function() {
    switch (this.safeBrowsingStatus_) {
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION:
        return 'cr:extension';
      default:
        return null;
    }
  },

  /**
   * @private
   * @return {ChildUiStatus}
   */
  getSafeBrowsingUiStatus_: function() {
    switch (this.safeBrowsingStatus_) {
      case SafetyCheckSafeBrowsingStatus.CHECKING:
        return ChildUiStatus.RUNNING;
      case SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD:
      case SafetyCheckSafeBrowsingStatus.ENABLED_ENHANCED:
        return ChildUiStatus.SAFE;
      case SafetyCheckSafeBrowsingStatus.ENABLED:
        // ENABLED is deprecated.
        assertNotReached();
      case SafetyCheckSafeBrowsingStatus.DISABLED:
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN:
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION:
        return ChildUiStatus.INFO;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getSafeBrowsingIcon_: function() {
    return this.getChildUiIcon_(this.getSafeBrowsingUiStatus_());
  },

  /**
   * @private
   * @return {?string}
   */
  getSafeBrowsingIconSrc_: function() {
    return this.getChildUiIconSrc_(this.getSafeBrowsingUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getSafeBrowsingIconClass_: function() {
    return this.getChildUiIconClass_(this.getSafeBrowsingUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getSafeBrowsingIconAriaLabel_: function() {
    return this.getChildUiIconAriaLabel_(this.getSafeBrowsingUiStatus_());
  },

  /** @private */
  onSafeBrowsingButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFETY_CHECK_SAFE_BROWSING_MANAGE);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ManageSafeBrowsing');

    Router.getInstance().navigateTo(routes.SECURITY);
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowExtensionsButton_: function() {
    switch (this.extensionsStatus_) {
      case SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_SOME_BY_USER:
        return true;
      default:
        return false;
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowExtensionsManagedIcon_: function() {
    return this.extensionsStatus_ ===
        SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN;
  },

  /** @private */
  onSafetyCheckExtensionsButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFETY_CHECK_EXTENSIONS_REVIEW);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ReviewExtensions');

    OpenWindowProxyImpl.getInstance().openURL('chrome://extensions');
  },

  /**
   * @private
   * @return {ChildUiStatus}
   */
  getExtensionsUiStatus_: function() {
    switch (this.extensionsStatus_) {
      case SafetyCheckExtensionsStatus.CHECKING:
        return ChildUiStatus.RUNNING;
      case SafetyCheckExtensionsStatus.ERROR:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN:
        return ChildUiStatus.INFO;
      case SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED:
        return ChildUiStatus.SAFE;
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_SOME_BY_USER:
        return ChildUiStatus.WARNING;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getExtensionsIcon_: function() {
    return this.getChildUiIcon_(this.getExtensionsUiStatus_());
  },

  /**
   * @private
   * @return {?string}
   */
  getExtensionsIconSrc_: function() {
    return this.getChildUiIconSrc_(this.getExtensionsUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getExtensionsIconClass_: function() {
    return this.getChildUiIconClass_(this.getExtensionsUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getExtensionsIconAriaLabel_: function() {
    return this.getChildUiIconAriaLabel_(this.getExtensionsUiStatus_());
  },

  /**
   * @private
   * @return {string}
   */
  getExtensionsButtonClass_: function() {
    switch (this.extensionsStatus_) {
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_SOME_BY_USER:
        return 'action-button';
      default:
        return '';
    }
  },
});
