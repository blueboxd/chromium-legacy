// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'confirm-password-change' is a dialog so that the user can
 * either confirm their old password, or confirm their new password (twice),
 * or both, as part of an in-session password change.
 * The dialog shows a spinner while it tries to change the password. This
 * spinner is also shown immediately in the case we are trying to change the
 * password using scraped data, and if this fails the spinner is hidden and
 * the main confirm dialog is shown.
 */

// TODO(https://crbug.com/930109): Add logic to show only some of the passwords
// fields if some of the passwords were successfully scraped.

/** @enum{number} */
const ValidationErrorType = {
  NO_ERROR: 0,
  MISSING_OLD_PASSWORD: 1,
  MISSING_NEW_PASSWORD: 2,
  MISSING_CONFIRM_NEW_PASSWORD: 3,
  PASSWORDS_DO_NOT_MATCH: 4,
  INCORRECT_OLD_PASSWORD: 5,
};

/** Default arguments that are used when not embedded in a dialog. */
const DefaultArgs = {
  showSpinnerInitially: false,
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

    errorString_:
        {type: String, computed: 'getErrorString_(currentValidationError_)'}
  },

  /** @override */
  attached: function() {
    // WebDialogUI class stores extra parameters in JSON in 'dialogArguments'
    // variable, if this webui is being rendered in a dialog.
    const argsJson = chrome.getVariableValue('dialogArguments');
    const args = argsJson ? JSON.parse(argsJson) : DefaultArgs;

    this.showSpinner_(args.showSpinnerInitially);

    this.addWebUIListener('incorrect-old-password', () => {
      this.onIncorrectOldPassword_();
    });
  },

  /**
   * @private
   */
  showSpinner_: function(showSpinner) {
    // Dialog is on top, spinner is underneath, so showing dialog hides spinner.
    if (showSpinner)
      this.$.dialog.close();
    else
      this.$.dialog.showModal();
  },

  /**
   * @private
   */
  onSaveTap_: function() {
    this.currentValidationError_ = this.findFirstError_();
    if (this.currentValidationError_ == ValidationErrorType.NO_ERROR) {
      chrome.send('changePassword', [this.old_password_, this.new_password_]);
      this.showSpinner_(true);
    }
  },

  /**
   * @private
   */
  onIncorrectOldPassword_: function() {
    // Only show the error if the user had previously typed an old password.
    // This is also called when an incorrect password was scraped. In that case
    // we hide the spinner and show the dialog so they can confirm.
    if (this.old_password_) {
      this.currentValidationError_ = ValidationErrorType.INCORRECT_OLD_PASSWORD;
      this.old_password_ = '';
    }
    this.showSpinner_(false);
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
    const err = this.currentValidationError_;
    return err == ValidationErrorType.MISSING_OLD_PASSWORD ||
        err == ValidationErrorType.INCORRECT_OLD_PASSWORD;
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
    const err = this.currentValidationError_;
    return err == ValidationErrorType.MISSING_CONFIRM_NEW_PASSWORD ||
        err == ValidationErrorType.PASSWORDS_DO_NOT_MATCH;
  },

  /**
   * @return {string}
   * @private
   */
  getErrorString_: function() {
    switch (this.currentValidationError_) {
      case ValidationErrorType.INCORRECT_OLD_PASSWORD:
        return this.i18n('incorrectPassword');
      case ValidationErrorType.PASSWORDS_DO_NOT_MATCH:
        return this.i18n('matchError');
      default:
        return '';
    }
  },
});
