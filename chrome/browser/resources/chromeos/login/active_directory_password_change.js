// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Active Directory password change screen.
 */

/**
 * Possible error states of the screen. Must be in the same order as
 * ActiveDirectoryPasswordChangeErrorState enum values.
 * @enum {number}
 */
var ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE = {
  WRONG_OLD_PASSWORD: 0,
  NEW_PASSWORD_REJECTED: 1,
};

Polymer({
  is: 'active-directory-password-change',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The user principal name.
     */
    username: String,

    /**
     * The old password.
     */
    oldPassword_: String,
    /**
     * The new password (first copy).
     */
    newPassword_: String,
    /**
     * The new password (second copy).
     */
    newPasswordRepeat_: String,

    /**
     * Indicates if old password is wrong.
     */
    oldPasswordWrong_: {
      type: Boolean,
      value: false,
    },
    /**
     * Indicates if new password is rejected.
     */
    newPasswordRejected_: {
      type: Boolean,
      value: false,
    },
    /**
     * Indicates if password is not repeated correctly.
     */
    passwordMismatch_: {
      type: Boolean,
      value: false,
    },
  },

  /** @public */
  reset: function() {
    this.$.animatedPages.selected = 0;
    this.resetInputFields_();
    this.updateNavigation_();
  },

  /** @private */
  resetInputFields_: function() {
    this.oldPassword = '';
    this.newPassword = '';
    this.newPasswordRepeat = '';

    this.oldPasswordWrong_ = false;
    this.newPasswordRejected_ = false;
    this.passwordMismatch_ = false;
  },

  /**
   * @public
   *  Invalidates a password input. Either the input for old password or for new
   *  password depending on passed error.
   * @param {ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE} error
   */
  setInvalid: function(error) {
    switch (error) {
      case ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE.WRONG_OLD_PASSWORD:
        this.oldPasswordWrong_ = true;
        break;
      case ACTIVE_DIRECTORY_PASSWORD_CHANGE_ERROR_STATE.NEW_PASSWORD_REJECTED:
        this.newPasswordRejected_ = true;
        break;
      default:
        console.error('Not handled error: ' + error);
    }
  },

  /** @private */
  onSubmit_: function() {
    if (!this.$.oldPassword.validate() || !this.$.newPassword.validate()) {
      return;
    }
    if (this.newPassword != this.newPasswordRepeat) {
      this.passwordMismatch_ = true;
      return;
    }
    this.$.animatedPages.selected++;
    this.updateNavigation_();
    var msg = {
      'username': this.username,
      'oldPassword': this.oldPassword,
      'newPassword': this.newPassword,
    };
    this.resetInputFields_();
    this.fire('authCompleted', msg);
  },

  /** @private */
  onClose_: function() {
    if (!this.$.navigation.closeVisible)
      return;
    this.fire('cancel');
  },

  /** @private */
  updateNavigation_: function() {
    this.$.navigation.closeVisible = (this.$.animatedPages.selected == 0);
  },
});
