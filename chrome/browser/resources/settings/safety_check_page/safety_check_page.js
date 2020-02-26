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

/**
 * @typedef {{
 *   safetyCheckComponent: SafetyCheckComponent,
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

  /** @override */
  attached: function() {
    this.safetyCheckBrowserProxy_ =
        settings.SafetyCheckBrowserProxyImpl.getInstance();

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
      case SafetyCheckComponent.UPDATES:
        this.updatesStatus_ = status;
        break;
      case SafetyCheckComponent.PASSWORDS:
        this.passwordsCompromisedCount_ = event['passwordsCompromised'];
        this.passwordsStatus_ = status;
        break;
      case SafetyCheckComponent.SAFE_BROWSING:
        this.safeBrowsingStatus_ = status;
        break;
      case SafetyCheckComponent.EXTENSIONS:
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
   * @return {boolean}
   */
  shouldShowChildren_: function() {
    return this.parentStatus_ != ParentStatus.BEFORE;
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
});
})();
