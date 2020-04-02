// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-page' is the settings page containing the browser
 * safety check.
 */

(function() {

/**
 * States of the safety check parent element.
 * @enum {number}
 */
const ParentStatus = {
  BEFORE: 0,
  CHECKING: 1,
  AFTER: 2,
};

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
 *   newState: settings.SafetyCheckUpdatesStatus,
 *   displayString: string,
 * }}
 */
let UpdatesChangedEvent;

/**
 * @typedef {{
 *   newState: settings.SafetyCheckPasswordsStatus,
 *   displayString: string,
 *   buttonString: string,
 * }}
 */
let PasswordsChangedEvent;

/**
 * @typedef {{
 *   newState: settings.SafetyCheckSafeBrowsingStatus,
 *   displayString: string,
 * }}
 */
let SafeBrowsingChangedEvent;

/**
 * @typedef {{
 *   newState: settings.SafetyCheckExtensionsStatus,
 *   displayString: string,
 * }}
 */
let ExtensionsChangedEvent;

Polymer({
  is: 'settings-safety-check-page',

  behaviors: [
    WebUIListenerBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check parent element.
     * @private {!ParentStatus}
     */
    parentStatus_: {
      type: Number,
      value: ParentStatus.BEFORE,
    },

    /**
     * Current state of the safety check updates element.
     * @private {!settings.SafetyCheckUpdatesStatus}
     */
    updatesStatus_: {
      type: Number,
      value: settings.SafetyCheckUpdatesStatus.CHECKING,
    },

    /**
     * Current state of the safety check passwords element.
     * @private {!settings.SafetyCheckPasswordsStatus}
     */
    passwordsStatus_: {
      type: Number,
      value: settings.SafetyCheckPasswordsStatus.CHECKING,
    },

    /**
     * Current state of the safety check safe browsing element.
     * @private {!settings.SafetyCheckSafeBrowsingStatus}
     */
    safeBrowsingStatus_: {
      type: Number,
      value: settings.SafetyCheckSafeBrowsingStatus.CHECKING,
    },

    /**
     * Current state of the safety check extensions element.
     * @private {!settings.SafetyCheckExtensionsStatus}
     */
    extensionsStatus_: {
      type: Number,
      value: settings.SafetyCheckExtensionsStatus.CHECKING,
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

    /**
     * UI string to display in the password button.
     * @private
     */
    passwordsButtonString_: String,
  },

  /** @private {settings.SafetyCheckBrowserProxy} */
  safetyCheckBrowserProxy_: null,

  /** @private {?settings.LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @private {?settings.MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /**
   * Timer ID for periodic update.
   * @private {number}
   */
  updateTimerId_: -1,

  /** @override */
  attached: function() {
    this.safetyCheckBrowserProxy_ =
        settings.SafetyCheckBrowserProxyImpl.getInstance();
    this.lifetimeBrowserProxy_ =
        settings.LifetimeBrowserProxyImpl.getInstance();
    this.metricsBrowserProxy_ = settings.MetricsBrowserProxyImpl.getInstance();

    // Register for safety check status updates.
    this.addWebUIListener(
        settings.SafetyCheckCallbackConstants.UPDATES_CHANGED,
        this.onSafetyCheckUpdatesChanged_.bind(this));
    this.addWebUIListener(
        settings.SafetyCheckCallbackConstants.PASSWORDS_CHANGED,
        this.onSafetyCheckPasswordsChanged_.bind(this));
    this.addWebUIListener(
        settings.SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED,
        this.onSafetyCheckSafeBrowsingChanged_.bind(this));
    this.addWebUIListener(
        settings.SafetyCheckCallbackConstants.EXTENSIONS_CHANGED,
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
    // Update UI.
    this.parentDisplayString_ = this.i18n('safetyCheckRunning');
    this.parentStatus_ = ParentStatus.CHECKING;
    // Reset all children states.
    this.updatesStatus_ = settings.SafetyCheckUpdatesStatus.CHECKING;
    this.passwordsStatus_ = settings.SafetyCheckPasswordsStatus.CHECKING;
    this.safeBrowsingStatus_ = settings.SafetyCheckSafeBrowsingStatus.CHECKING;
    this.extensionsStatus_ = settings.SafetyCheckExtensionsStatus.CHECKING;
    // Display running-status for safety check elements.
    this.updatesDisplayString_ = '';
    this.passwordsDisplayString_ = '';
    this.safeBrowsingDisplayString_ = '';
    this.extensionsDisplayString_ = '';
    // Trigger safety check.
    this.safetyCheckBrowserProxy_.runSafetyCheck();
    // Readout new safety check status via accessibility.
    this.fire('iron-announce', {
      text: this.i18n('safetyCheckAriaLiveRunning'),
    });
  },

  /** @private */
  updateParentFromChildren_: function() {
    // If all children elements received updates: update parent element.
    if (this.updatesStatus_ != settings.SafetyCheckUpdatesStatus.CHECKING &&
        this.passwordsStatus_ != settings.SafetyCheckPasswordsStatus.CHECKING &&
        this.safeBrowsingStatus_ !=
            settings.SafetyCheckSafeBrowsingStatus.CHECKING &&
        this.extensionsStatus_ !=
            settings.SafetyCheckExtensionsStatus.CHECKING) {
      // Update UI.
      this.parentStatus_ = ParentStatus.AFTER;
      // Start periodic safety check parent ran string updates.
      const timestamp = Date.now();
      const update = async () => {
        this.parentDisplayString_ =
            await this.safetyCheckBrowserProxy_.getParentRanDisplayString(
                timestamp);
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
    this.updateParentFromChildren_();
  },

  /**
   * @param {!PasswordsChangedEvent} event
   * @private
   */
  onSafetyCheckPasswordsChanged_: function(event) {
    this.passwordsDisplayString_ = event.displayString;
    this.passwordsButtonString_ = event.buttonString;
    this.passwordsStatus_ = event.newState;
    this.updateParentFromChildren_();
  },

  /**
   * @param {!SafeBrowsingChangedEvent} event
   * @private
   */
  onSafetyCheckSafeBrowsingChanged_: function(event) {
    this.safeBrowsingStatus_ = event.newState;
    this.safeBrowsingDisplayString_ = event.displayString;
    this.updateParentFromChildren_();
  },

  /**
   * @param {!ExtensionsChangedEvent} event
   * @private
   */
  onSafetyCheckExtensionsChanged_: function(event) {
    this.extensionsDisplayString_ = event.displayString;
    this.extensionsStatus_ = event.newState;
    this.updateParentFromChildren_();
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowParentButton_: function() {
    return this.parentStatus_ === ParentStatus.BEFORE;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowParentIconButton_: function() {
    return this.parentStatus_ !== ParentStatus.BEFORE;
  },

  /** @private */
  onRunSafetyCheckClick_: function() {
    settings.HatsBrowserProxyImpl.getInstance().tryShowSurvey();

    this.runSafetyCheck_();
    Polymer.dom.flush();
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
    return this.parentStatus_ != ParentStatus.BEFORE;
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
    return this.updatesStatus_ === settings.SafetyCheckUpdatesStatus.RELAUNCH;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowUpdatesManagedIcon_: function() {
    return this.updatesStatus_ ===
        settings.SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN;
  },

  /** @private */
  onSafetyCheckUpdatesButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckPageHistogram(
        settings.SafetyCheckElementInteractions.SAFETY_CHECK_UPDATES_RELAUNCH);
    this.metricsBrowserProxy_.recordAction('SafetyCheck.Updates.Relaunch');

    this.lifetimeBrowserProxy_.relaunch();
  },

  /**
   * @return {ChildUiStatus}
   * @private
   */
  getUpdatesUiStatus_: function() {
    switch (this.updatesStatus_) {
      case settings.SafetyCheckUpdatesStatus.CHECKING:
      case settings.SafetyCheckUpdatesStatus.UPDATING:
        return ChildUiStatus.RUNNING;
      case settings.SafetyCheckUpdatesStatus.UPDATED:
        return ChildUiStatus.SAFE;
      case settings.SafetyCheckUpdatesStatus.RELAUNCH:
      case settings.SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
      case settings.SafetyCheckUpdatesStatus.FAILED_OFFLINE:
        return ChildUiStatus.INFO;
      case settings.SafetyCheckUpdatesStatus.FAILED:
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
    return this.passwordsStatus_ ===
        settings.SafetyCheckPasswordsStatus.COMPROMISED;
  },

  /**
   * @private
   * @return {ChildUiStatus}
   */
  getPasswordsUiStatus_: function() {
    switch (this.passwordsStatus_) {
      case settings.SafetyCheckPasswordsStatus.CHECKING:
        return ChildUiStatus.RUNNING;
      case settings.SafetyCheckPasswordsStatus.SAFE:
        return ChildUiStatus.SAFE;
      case settings.SafetyCheckPasswordsStatus.COMPROMISED:
        return ChildUiStatus.WARNING;
      case settings.SafetyCheckPasswordsStatus.OFFLINE:
      case settings.SafetyCheckPasswordsStatus.NO_PASSWORDS:
      case settings.SafetyCheckPasswordsStatus.SIGNED_OUT:
      case settings.SafetyCheckPasswordsStatus.QUOTA_LIMIT:
      case settings.SafetyCheckPasswordsStatus.ERROR:
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
    this.metricsBrowserProxy_.recordSafetyCheckPageHistogram(
        settings.SafetyCheckElementInteractions.SAFETY_CHECK_PASSWORDS_MANAGE);
    this.metricsBrowserProxy_.recordAction('SafetyCheck.Passwords.Manage');

    settings.Router.getInstance().navigateTo(settings.routes.CHECK_PASSWORDS);
    PasswordManagerImpl.getInstance().recordPasswordCheckReferrer(
        PasswordManagerProxy.PasswordCheckReferrer.SAFETY_CHECK);
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowSafeBrowsingButton_: function() {
    return this.safeBrowsingStatus_ ===
        settings.SafetyCheckSafeBrowsingStatus.DISABLED;
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
      case settings.SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case settings.SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION:
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
      case settings.SafetyCheckSafeBrowsingStatus.CHECKING:
        return ChildUiStatus.RUNNING;
      case settings.SafetyCheckSafeBrowsingStatus.ENABLED:
        return ChildUiStatus.SAFE;
      case settings.SafetyCheckSafeBrowsingStatus.DISABLED:
      case settings.SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN:
      case settings.SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION:
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
    this.metricsBrowserProxy_.recordSafetyCheckPageHistogram(
        settings.SafetyCheckElementInteractions
            .SAFETY_CHECK_SAFE_BROWSING_MANAGE);
    this.metricsBrowserProxy_.recordAction('SafetyCheck.SafeBrowsing.Manage');

    settings.Router.getInstance().navigateTo(settings.routes.SECURITY);
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowExtensionsButton_: function() {
    switch (this.extensionsStatus_) {
      case settings.SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED:
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_ALL_BY_USER:
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_SOME_BY_USER:
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
        settings.SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN;
  },

  /** @private */
  onSafetyCheckExtensionsButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckPageHistogram(
        settings.SafetyCheckElementInteractions.SAFETY_CHECK_EXTENSIONS_REVIEW);
    this.metricsBrowserProxy_.recordAction('SafetyCheck.Extensions.Review');

    settings.OpenWindowProxyImpl.getInstance().openURL('chrome://extensions');
  },

  /**
   * @private
   * @return {ChildUiStatus}
   */
  getExtensionsUiStatus_: function() {
    switch (this.extensionsStatus_) {
      case settings.SafetyCheckExtensionsStatus.CHECKING:
        return ChildUiStatus.RUNNING;
      case settings.SafetyCheckExtensionsStatus.ERROR:
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_ALL_BY_ADMIN:
        return ChildUiStatus.INFO;
      case settings.SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS:
      case settings.SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED:
        return ChildUiStatus.SAFE;
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_ALL_BY_USER:
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_SOME_BY_USER:
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
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_ALL_BY_USER:
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_SOME_BY_USER:
        return 'action-button';
      default:
        return '';
    }
  },
});
})();
