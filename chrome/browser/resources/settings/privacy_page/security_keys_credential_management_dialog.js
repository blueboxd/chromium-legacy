// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * @fileoverview 'settings-security-keys-credential-management-dialog' is a
 * dialog for viewing and erasing credentials stored on a security key.
 */
Polymer({
  is: 'settings-security-keys-credential-management-dialog',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The ID of the element currently shown in the dialog.
     * @private
     */
    dialogPage_: {
      type: String,
      value: 'initial',
      observer: 'dialogPageChanged_',
    },

    /**
     * The list of credentials displayed in the dialog.
     * @private {!Array<!Credential>}
     */
    credentials_: {
      type: Array,
      value: () => [],
    },

    /**
     * The message displayed on the "error" dialog page.
     * @private
     */
    errorMsg_: String,

    /** @private */
    cancelButtonVisible_: Boolean,

    /** @private */
    confirmButtonVisible_: Boolean,

    /** @private */
    closeButtonVisible_: Boolean,
  },

  /** @private {?settings.SecurityKeysBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
    this.addWebUIListener(
        'security-keys-credential-management-finished',
        this.onError_.bind(this));
    this.browserProxy_ = settings.SecurityKeysBrowserProxyImpl.getInstance();
    this.browserProxy_.startCredentialManagement().then(
        this.collectPin_.bind(this));
  },

  /** @private */
  collectPin_: function() {
    this.dialogPage_ = 'pinPrompt';
    this.$.pin.focus();
  },

  /**
   * @private
   * @param {string} error
   */
  onError_: function(error) {
    this.errorMsg_ = error;
    this.dialogPage_ = 'error';
  },

  /** @private */
  submitPIN_: function() {
    if (!this.$.pin.validate()) {
      return;
    }
    this.browserProxy_.credentialManagementProvidePIN(this.$.pin.value)
        .then((retries) => {
          if (retries != null) {
            this.$.pin.showIncorrectPINError(retries);
            this.collectPin_();
            return;
          }
          this.browserProxy_.credentialManagementEnumerate().then(
              this.onCredentials_.bind(this));
        });
  },

  /**
   * @param {number} retries
   * @return {string} localized error string for an invalid PIN attempt and a
   *     given number of remaining retries.
   */
  pinRetriesError_: function(retries) {
    // Warn the user if the number of retries is getting low.
    if (1 < retries && retries <= 3) {
      return this.i18n('securityKeysPINIncorrectRetriesPl', retries.toString());
    }
    return this.i18n(
        retries == 1 ? 'securityKeysPINIncorrectRetriesSin' :
                       'securityKeysPINIncorrect');
  },

  /**
   * @private
   * @param {!Array<!Credential>} credentials
   */
  onCredentials_: function(credentials) {
    this.credentials_ = credentials;
    this.$.credentialList.fire('iron-resize');
    this.dialogPage_ = 'credentials';
  },

  /** @private */
  dialogPageChanged_: function() {
    switch (this.dialogPage_) {
      case 'initial':
        this.cancelButtonVisible_ = true;
        this.confirmButtonVisible_ = false;
        this.closeButtonVisible_ = false;
        break;
      case 'pinPrompt':
        this.cancelButtonVisible_ = true;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        break;
      case 'credentials':
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = false;
        this.closeButtonVisible_ = true;
        break;
      case 'error':
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = false;
        this.closeButtonVisible_ = true;
        break;
    }
  },

  /** @private */
  confirmButtonClick_: function() {
    assert(this.dialogPage_ == 'pinPrompt');
    this.submitPIN_();
  },

  /** @private */
  close_: function() {
    this.$.dialog.close();
  },

  /**
   * Stringifies the user entity of a Credential for display in the dialog.
   * @private
   * @param {!Credential} credential
   * @return {string}
   */
  formatUser_: function(credential) {
    if (this.isEmpty_(credential.userDisplayName)) {
      return credential.userName;
    }
    return `${credential.userDisplayName} (${credential.userName})`;
  },

  /** @private */
  onDialogClosed_: function() {
    this.browserProxy_.close();
  },

  /**
   * @private
   * @param {?string} str
   * @return {boolean} true if the string doesn't exist or is empty.
   */
  isEmpty_: function(str) {
    return !str || str.length == 0;
  },

  /**
   * @private
   * @param {?Array<Object>} list
   * @return {boolean} true if the list exists and has items.
   */
  hasSome_: function(list) {
    return !!(list && list.length);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIronSelect_: function(e) {
    // Prevent this event from bubbling since it is unnecessarily triggering the
    // listener within settings-animated-pages.
    e.stopPropagation();
  },
});
})();
