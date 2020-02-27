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
   * Values used to identify safety check components in the callback dictionary.
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
  // #cr_define_end
  return {SafetyCheckComponent};
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
 *   safetyCheckComponent: settings.SafetyCheckComponent,
 *   newState: number,
 *   passwordsCompromised: (number|undefined),
 *   badExtensions: (number|undefined),
 * }}
 */
/* #export */ let SafetyCheckStatusChangedEvent;

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
     * Number of password compromises.
     * @private {number}
     */
    passwordsCompromisedCount_: {
      type: Number,
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
     * Number of bad extensions.
     * @private {number}
     */
    badExtensionsCount_: {
      type: Number,
    },
  },

  /** @private {settings.SafetyCheckBrowserProxy} */
  safetyCheckBrowserProxy_: null,

  /** @private {?settings.LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.safetyCheckBrowserProxy_ =
        settings.SafetyCheckBrowserProxyImpl.getInstance();
    this.lifetimeBrowserProxy_ =
        settings.LifetimeBrowserProxyImpl.getInstance();

    // Register for safety check status updates.
    this.addWebUIListener(
        'safety-check-status-changed',
        this.onSafetyCheckStatusUpdate_.bind(this));
  },

  /**
   * Triggers the safety check.
   * @private
   */
  runSafetyCheck_: function() {
    // Update UI.
    this.parentStatus_ = ParentStatus.CHECKING;
    // Reset all children states.
    this.updatesStatus_ = settings.SafetyCheckUpdatesStatus.CHECKING;
    this.passwordsStatus_ = settings.SafetyCheckPasswordsStatus.CHECKING;
    this.safeBrowsingStatus_ = settings.SafetyCheckSafeBrowsingStatus.CHECKING;
    this.extensionsStatus_ = settings.SafetyCheckExtensionsStatus.CHECKING;
    // Trigger safety check.
    this.safetyCheckBrowserProxy_.runSafetyCheck();
  },

  /**
   * Safety check callback to update UI from safety check result.
   * @param {SafetyCheckStatusChangedEvent} event
   * @private
   */
  onSafetyCheckStatusUpdate_: function(event) {
    const status = event['newState'];
    switch (event.safetyCheckComponent) {
      case settings.SafetyCheckComponent.UPDATES:
        this.updatesStatus_ = status;
        break;
      case settings.SafetyCheckComponent.PASSWORDS:
        this.passwordsCompromisedCount_ = event['passwordsCompromised'];
        this.passwordsStatus_ = status;
        break;
      case settings.SafetyCheckComponent.SAFE_BROWSING:
        this.safeBrowsingStatus_ = status;
        break;
      case settings.SafetyCheckComponent.EXTENSIONS:
        this.badExtensionsCount_ = event['badExtensions'];
        this.extensionsStatus_ = status;
        break;
      default:
        assertNotReached();
    }

    // If all children elements received updates: update parent element.
    if (this.areAllChildrenStatesSet_()) {
      this.parentStatus_ = ParentStatus.AFTER;
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  areAllChildrenStatesSet_: function() {
    return this.updatesStatus_ != settings.SafetyCheckUpdatesStatus.CHECKING &&
        this.passwordsStatus_ != settings.SafetyCheckPasswordsStatus.CHECKING &&
        this.safeBrowsingStatus_ !=
        settings.SafetyCheckSafeBrowsingStatus.CHECKING &&
        this.extensionsStatus_ != settings.SafetyCheckExtensionsStatus.CHECKING;
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

  /**
   * @private
   * @return {?string}
   */
  getParentIcon_: function() {
    switch (this.parentStatus_) {
      case ParentStatus.BEFORE:
      case ParentStatus.AFTER:
        return 'settings:assignment';
      case ParentStatus.CHECKING:
        return null;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getParentIconSrc_: function() {
    switch (this.parentStatus_) {
      case ParentStatus.BEFORE:
      case ParentStatus.AFTER:
        return null;
      case ParentStatus.CHECKING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {string}
   */
  getParentIconClass_: function() {
    switch (this.parentStatus_) {
      case ParentStatus.BEFORE:
      case ParentStatus.CHECKING:
        return 'icon-blue';
      default:
        return '';
    }
  },

  /** @private */
  onRunSafetyCheckClick_: function() {
    this.runSafetyCheck_();
  },

  /**
   * @private
   * @return {string}
   */
  getParentPrimaryLabelText_: function() {
    switch (this.parentStatus_) {
      case ParentStatus.BEFORE:
        return this.i18n('safetyCheckParentPrimaryLabelBefore');
      case ParentStatus.CHECKING:
        return this.i18n('safetyCheckParentPrimaryLabelChecking');
      case ParentStatus.AFTER:
        return this.i18n('safetyCheckParentPrimaryLabelAfter');
      default:
        assertNotReached();
    }
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
   * @return {string}
   */
  getUpdatesSubLabelText_: function() {
    switch (this.updatesStatus_) {
      case settings.SafetyCheckUpdatesStatus.CHECKING:
        return '';
      case settings.SafetyCheckUpdatesStatus.UPDATED:
        return this.i18n('aboutUpgradeUpToDate');
      case settings.SafetyCheckUpdatesStatus.UPDATING:
        return this.i18n('aboutUpgradeUpdating');
      case settings.SafetyCheckUpdatesStatus.RELAUNCH:
        return this.i18n('aboutUpgradeRelaunch');
      case settings.SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
        return this.i18n('safetyCheckUpdatesSubLabelDisabledByAdmin');
      case settings.SafetyCheckUpdatesStatus.FAILED_OFFLINE:
        return this.i18n('safetyCheckUpdatesSubLabelFailedOffline');
      case settings.SafetyCheckUpdatesStatus.FAILED:
        return this.i18n('safetyCheckUpdatesSubLabelFailed');
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowPasswordsButton_: function() {
    switch (this.passwordsStatus_) {
      case settings.SafetyCheckPasswordsStatus.COMPROMISED:
      case settings.SafetyCheckPasswordsStatus.ERROR:
        return true;
      default:
        return false;
    }
  },

  /**
   * @private
   * @return {string}
   */
  getPasswordsSubLabelText_: function() {
    switch (this.passwordsStatus_) {
      case settings.SafetyCheckPasswordsStatus.CHECKING:
        return '';
      case settings.SafetyCheckPasswordsStatus.SAFE:
        return this.i18n('safetyCheckPasswordsSubLabelSafe');
      case settings.SafetyCheckPasswordsStatus.COMPROMISED:
        if (this.passwordsCompromisedCount_ == 1) {
          return this.i18n('safetyCheckPasswordsSubLabelCompromisedSingular');
        }
        return this.i18n(
            'safetyCheckPasswordsSubLabelCompromisedPlural',
            this.passwordsCompromisedCount_);
      case settings.SafetyCheckPasswordsStatus.OFFLINE:
        return this.i18n('safetyCheckPasswordsSubLabelOffline');
      case settings.SafetyCheckPasswordsStatus.NO_PASSWORDS:
        return this.i18n('safetyCheckPasswordsSubLabelNoPasswords');
      case settings.SafetyCheckPasswordsStatus.SIGNED_OUT:
        return this.i18n('safetyCheckPasswordsSubLabelSignedOut');
      case settings.SafetyCheckPasswordsStatus.QUOTA_LIMIT:
        return this.i18n('safetyCheckPasswordsSubLabelQuotaLimit');
      case settings.SafetyCheckPasswordsStatus.TOO_MANY_PASSWORDS:
        return this.i18n('safetyCheckPasswordsSubLabelTooManyPasswords');
      case settings.SafetyCheckPasswordsStatus.ERROR:
        return this.i18n('safetyCheckPasswordsSubLabelError');
      default:
        assertNotReached();
    }
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
      case settings.SafetyCheckPasswordsStatus.TOO_MANY_PASSWORDS:
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

  /**
   * @private
   * @return {?string}
   */
  getPasswordsButtonText_: function() {
    switch (this.passwordsStatus_) {
      case settings.SafetyCheckPasswordsStatus.COMPROMISED:
        return this.i18n('safetyCheckPasswordsButtonCompromised');
      case settings.SafetyCheckPasswordsStatus.ERROR:
        return this.i18n('safetyCheckPasswordsButtonError');
      default:
        return null;
    }
  },

  /** @private */
  onPasswordsButtonClick_: function() {
    switch (this.passwordsStatus_) {
      case settings.SafetyCheckPasswordsStatus.COMPROMISED:
        // TODO(crbug.com/1010001): Implement once behavior has been agreed on.
        break;
      case settings.SafetyCheckPasswordsStatus.ERROR:
        this.runSafetyCheck_();
        break;
      default:
        break;
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowExtensionsButton_: function() {
    switch (this.extensionsStatus_) {
      case settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_ON:
      case settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_OFF:
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
        settings.SafetyCheckExtensionsStatus.MANAGED_BY_ADMIN;
  },

  /** @private */
  onSafetyCheckExtensionsButtonClicked_: function() {
    // TODO(crbug.com/1010001): Implement once behavior has been agreed on.
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
      case settings.SafetyCheckExtensionsStatus.MANAGED_BY_ADMIN:
        return 'cr:info';
      case settings.SafetyCheckExtensionsStatus.SAFE:
      case settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_OFF:
        return 'cr:check';
      case settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_ON:
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
      case settings.SafetyCheckExtensionsStatus.SAFE:
      case settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_OFF:
        return 'icon-blue';
      case settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_ON:
        return 'icon-red';
      default:
        return '';
    }
  },

  /**
   * @private
   * @return {string}
   */
  getExtensionsSubLabelText_: function() {
    switch (this.extensionsStatus_) {
      case settings.SafetyCheckExtensionsStatus.CHECKING:
        return '';
      case settings.SafetyCheckExtensionsStatus.ERROR:
        return this.i18n('safetyCheckExtensionsSubLabelError');
      case settings.SafetyCheckExtensionsStatus.SAFE:
        return this.i18n('safetyCheckExtensionsSubLabelSafe');
      case settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_ON:
        if (this.badExtensionsCount_ == 1) {
          return this.i18n(
              'safetyCheckExtensionsSubLabelBadExtensionsOnSingular');
        }
        return this.i18n(
            'safetyCheckExtensionsSubLabelBadExtensionsOnPlural',
            this.badExtensionsCount_);
      case settings.SafetyCheckExtensionsStatus.BAD_EXTENSIONS_OFF:
        if (this.badExtensionsCount_ == 1) {
          return this.i18n(
              'safetyCheckExtensionsSubLabelBadExtensionsOffSingular');
        }
        return this.i18n(
            'safetyCheckExtensionsSubLabelBadExtensionsOffPlural',
            this.badExtensionsCount_);
      case settings.SafetyCheckExtensionsStatus.MANAGED_BY_ADMIN:
        if (this.badExtensionsCount_ == 1) {
          return this.i18n(
              'safetyCheckExtensionsSubLabelManagedByAdminSingular');
        }
        return this.i18n(
            'safetyCheckExtensionsSubLabelManagedByAdminPlural',
            this.badExtensionsCount_);
      default:
        assertNotReached();
    }
  },
});
})();
