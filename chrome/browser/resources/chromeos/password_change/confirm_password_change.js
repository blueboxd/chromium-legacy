// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'confirm-password-change' is a dialog so that the user can
 * either confirm their old password, or confirm their new password (twice),
 * or both, as part of an in-session password change.
 */

// TODO(https://crbug.com/930109): Logic is not done. Need to add logic to
// show a spinner, to show only some of the password fields,
// and to handle clicks on the save button.

/** @enum{number} */
const ValidationErrorType = {
  NO_ERROR: 0,
  MISSING_OLD_PASSWORD: 1,
  MISSING_NEW_PASSWORD: 2,
  MISSING_CONFIRM_NEW_PASSWORD: 3,
  PASSWORDS_DO_NOT_MATCH: 4,
};

Polymer({
  is: 'confirm-password-change',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private {string} */
    old_password_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    new_password_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    confirm_new_password_: {
      type: String,
      value: '',
    },

    /** @private {!ValidationErrorType} */
    currentValidationError_: {
      type: Number,
      value: ValidationErrorType.NO_ERROR,
    },
  },

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
  },

  /**
   * @private
   */
  onSaveTap_: function() {
    this.currentValidationError_ = this.findFirstError_();
    if (this.currentValidationError_ == ValidationErrorType.NO_ERROR) {
      chrome.send('changePassword', [this.old_password_, this.new_password_]);
    }
  },

  /**
   * @return {!ValidationErrorType}
   * @private
   */
  findFirstError_: function() {
    if (!this.old_password_) {
      return ValidationErrorType.MISSING_OLD_PASSWORD;
    }
    if (!this.new_password_) {
      return ValidationErrorType.MISSING_NEW_PASSWORD;
    }
    if (!this.confirm_new_password_) {
      return ValidationErrorType.MISSING_CONFIRM_NEW_PASSWORD;
    }
    if (this.new_password_ != this.confirm_new_password_) {
      return ValidationErrorType.PASSWORDS_DO_NOT_MATCH;
    }
    return ValidationErrorType.NO_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  invalidOldPassword_: function() {
    return this.currentValidationError_ ==
        ValidationErrorType.MISSING_OLD_PASSWORD;
  },

  /**
   * @return {boolean}
   * @private
   */
  invalidNewPassword_: function() {
    return this.currentValidationError_ ==
        ValidationErrorType.MISSING_NEW_PASSWORD;
  },

  /**
   * @return {boolean}
   * @private
   */
  invalidConfirmNewPassword_: function() {
    return this.currentValidationError_ ==
        ValidationErrorType.MISSING_CONFIRM_NEW_PASSWORD ||
        this.passwordsDoNotMatch_();
  },

  /**
   * @return {boolean}
   * @private
   */
  passwordsDoNotMatch_: function() {
    return this.currentValidationError_ ==
        ValidationErrorType.PASSWORDS_DO_NOT_MATCH;
  },
});
