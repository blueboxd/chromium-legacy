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
  onRunSafetyCheckClick_: function() {
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
        this.passwordsStatus_ = status;
        this.passwordsCompromisedCount_ = event['passwordsCompromised'];
        break;
      case SafetyCheckComponent.SAFE_BROWSING:
        this.safeBrowsingStatus_ = status;
        break;
      case SafetyCheckComponent.EXTENSIONS:
        this.extensionsStatus_ = status;
        this.badExtensionsCount_ = event['badExtensions'];
        break;
      default:
        assertNotReached();
    }

    // If all children elements received updates: update parent element.
    if (this.updatesStatus_ != settings.SafetyCheckUpdatesStatus.CHECKING &&
        this.passwordsStatus_ != settings.SafetyCheckPasswordsStatus.CHECKING &&
        this.safeBrowsingStatus_ !=
            settings.SafetyCheckSafeBrowsingStatus.CHECKING &&
        this.extensionsStatus_ !=
            settings.SafetyCheckExtensionsStatus.CHECKING) {
      this.parentStatus_ = ParentStatus.AFTER;
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  showParentButton_: function() {
    return this.parentStatus_ == ParentStatus.BEFORE;
  },

  /**
   * @private
   * @return {boolean}
   */
  showParentIconButton_: function() {
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
      case ParentStatus.AFTER:
        return '';
      default:
        assertNotReached();
    }
  },
});
})();
