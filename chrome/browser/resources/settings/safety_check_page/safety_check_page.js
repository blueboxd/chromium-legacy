// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-page' is the settings page containing the browser
 * safety check.
 */
cr.define('settings', function() {
  /**
   * Values used to identify safety check components in the callback event.
   * Needs to be kept in sync with SafetyCheckComponent in
   * chrome/browser/ui/webui/settings/safety_check_handler.h
   * @enum {number}
   */
  const SafetyCheckComponent = {
    UPDATES: 0,
    PASSWORDS: 1,
    SAFE_BROWSING: 2,
    EXTENSIONS: 3,
  };

  /**
   * Constants used in safety check C++ to JS communication.
   * Their values need be kept in sync with their counterparts in
   * chrome/browser/ui/webui/settings/safety_check_handler.h and
   * chrome/browser/ui/webui/settings/safety_check_handler.cc
   * @enum {string}
   */
  const SafetyCheckCallbackConstants = {
    UPDATES_CHANGED: 'safety-check-updates-status-changed',
    PASSWORDS_CHANGED: 'safety-check-passwords-status-changed',
    SAFE_BROWSING_CHANGED: 'safety-check-safe-browsing-status-changed',
    EXTENSIONS_CHANGED: 'safety-check-extensions-status-changed',
  };

  // #cr_define_end
  return {
    SafetyCheckComponent,
    SafetyCheckCallbackConstants,
  };
});

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
    return this.parentStatus_ == ParentStatus.BEFORE;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowParentIconButton_: function() {
    return this.parentStatus_ == ParentStatus.AFTER;
  },

  /** @private */
  onRunSafetyCheckClick_: function() {
    settings.HatsBrowserProxyImpl.getInstance().tryShowSurvey();

    this.runSafetyCheck_();
    this.focusParent_();
  },

  /** @private */
  focusParent_() {
    const parent =
        /** @type {!Element} */ (this.$$('#safetyCheckParent'));
    parent.focus();
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowChildren_: function() {
    return this.parentStatus_ != ParentStatus.BEFORE;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowUpdatesButton_: function() {
    return this.updatesStatus_ == settings.SafetyCheckUpdatesStatus.RELAUNCH;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowUpdatesManagedIcon_: function() {
    return this.updatesStatus_ ==
        settings.SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN;
  },

  /** @private */
  onSafetyCheckUpdatesButtonClicked_: function() {
    this.lifetimeBrowserProxy_.relaunch();
  },

  /**
   * @private
   * @return {?string}
   */
  getUpdatesIcon_: function() {
    switch (this.updatesStatus_) {
      case settings.SafetyCheckUpdatesStatus.CHECKING:
      case settings.SafetyCheckUpdatesStatus.UPDATING:
        return null;
      case settings.SafetyCheckUpdatesStatus.UPDATED:
        return 'cr:check';
      case settings.SafetyCheckUpdatesStatus.RELAUNCH:
      case settings.SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
      case settings.SafetyCheckUpdatesStatus.FAILED_OFFLINE:
        return 'cr:info';
      case settings.SafetyCheckUpdatesStatus.FAILED:
        return 'cr:warning';
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getUpdatesIconSrc_: function() {
    switch (this.updatesStatus_) {
      case settings.SafetyCheckUpdatesStatus.CHECKING:
      case settings.SafetyCheckUpdatesStatus.UPDATING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },

  /**
   * @private
   * @return {string}
   */
  getUpdatesIconClass_: function() {
    switch (this.updatesStatus_) {
      case settings.SafetyCheckUpdatesStatus.CHECKING:
      case settings.SafetyCheckUpdatesStatus.UPDATED:
      case settings.SafetyCheckUpdatesStatus.UPDATING:
        return 'icon-blue';
      case settings.SafetyCheckUpdatesStatus.FAILED:
        return 'icon-red';
      default:
        return '';
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowPasswordsButton_: function() {
    return this.passwordsStatus_ ==
        settings.SafetyCheckPasswordsStatus.COMPROMISED;
  },

  /**
   * @private
   * @return {?string}
   */
  getPasswordsIcon_: function() {
    switch (this.passwordsStatus_) {
      case settings.SafetyCheckPasswordsStatus.CHECKING:
        return null;
      case settings.SafetyCheckPasswordsStatus.SAFE:
        return 'cr:check';
      case settings.SafetyCheckPasswordsStatus.COMPROMISED:
        return 'cr:warning';
      case settings.SafetyCheckPasswordsStatus.OFFLINE:
      case settings.SafetyCheckPasswordsStatus.NO_PASSWORDS:
      case settings.SafetyCheckPasswordsStatus.SIGNED_OUT:
      case settings.SafetyCheckPasswordsStatus.QUOTA_LIMIT:
      case settings.SafetyCheckPasswordsStatus.ERROR:
        return 'cr:info';
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getPasswordsIconSrc_: function() {
    switch (this.passwordsStatus_) {
      case settings.SafetyCheckPasswordsStatus.CHECKING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },

  /**
   * @private
   * @return {string}
   */
  getPasswordsIconClass_: function() {
    switch (this.passwordsStatus_) {
      case settings.SafetyCheckPasswordsStatus.CHECKING:
      case settings.SafetyCheckPasswordsStatus.SAFE:
        return 'icon-blue';
      case settings.SafetyCheckPasswordsStatus.COMPROMISED:
        return 'icon-red';
      default:
        return '';
    }
  },

  /** @private */
  onPasswordsButtonClick_: function() {
    settings.Router.getInstance().navigateTo(
        loadTimeData.getBoolean('enablePasswordCheck') ?
            settings.routes.CHECK_PASSWORDS :
            settings.routes.PASSWORDS);
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowSafeBrowsingButton_: function() {
    return this.safeBrowsingStatus_ ==
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
   * @return {?string}
   */
  getSafeBrowsingIcon_: function() {
    switch (this.safeBrowsingStatus_) {
      case settings.SafetyCheckSafeBrowsingStatus.CHECKING:
        return null;
      case settings.SafetyCheckSafeBrowsingStatus.ENABLED:
        return 'cr:check';
      case settings.SafetyCheckSafeBrowsingStatus.DISABLED:
      case settings.SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN:
      case settings.SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION:
        return 'cr:info';
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getSafeBrowsingIconSrc_: function() {
    switch (this.safeBrowsingStatus_) {
      case settings.SafetyCheckSafeBrowsingStatus.CHECKING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },

  /**
   * @private
   * @return {string}
   */
  getSafeBrowsingIconClass_: function() {
    switch (this.safeBrowsingStatus_) {
      case settings.SafetyCheckSafeBrowsingStatus.CHECKING:
      case settings.SafetyCheckSafeBrowsingStatus.ENABLED:
        return 'icon-blue';
      default:
        return '';
    }
  },

  /** @private */
  onSafeBrowsingButtonClick_: function() {
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
    return this.extensionsStatus_ ==
        settings.SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN;
  },

  /** @private */
  onSafetyCheckExtensionsButtonClicked_: function() {
    settings.OpenWindowProxyImpl.getInstance().openURL('chrome://extensions');
  },

  /**
   * @private
   * @return {?string}
   */
  getExtensionsIcon_: function() {
    switch (this.extensionsStatus_) {
      case settings.SafetyCheckExtensionsStatus.CHECKING:
        return null;
      case settings.SafetyCheckExtensionsStatus.ERROR:
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_ALL_BY_ADMIN:
        return 'cr:info';
      case settings.SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS:
      case settings.SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED:
        return 'cr:check';
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_ALL_BY_USER:
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_SOME_BY_USER:
        return 'cr:warning';
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getExtensionsIconSrc_: function() {
    switch (this.extensionsStatus_) {
      case settings.SafetyCheckExtensionsStatus.CHECKING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },

  /**
   * @private
   * @return {string}
   */
  getExtensionsIconClass_: function() {
    switch (this.extensionsStatus_) {
      case settings.SafetyCheckExtensionsStatus.CHECKING:
      case settings.SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS:
      case settings.SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED:
        return 'icon-blue';
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_ALL_BY_USER:
      case settings.SafetyCheckExtensionsStatus
          .BLOCKLISTED_REENABLED_SOME_BY_USER:
        return 'icon-red';
      default:
        return '';
    }
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
