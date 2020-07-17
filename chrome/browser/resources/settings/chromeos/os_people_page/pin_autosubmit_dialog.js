// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'pin-autosubmit-confirmation-dialog' is a confirmation dialog that pops up
 * when the user chooses to enable PIN auto submit. The user is prompted to
 * enter their current PIN and if it matches the feature is enabled.
 *
 */

Polymer({
  is: 'settings-pin-autosubmit-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The current PIN keyboard value.
     * @private
     */
    pinValue_: {
      type: String,
    },

    /**
     * Whether the entered PIN is incorrect.
     * @private
     */
    pinIncorrect_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether there is a request in process already. Disables the
     * buttons, but leaves the cancel button actionable.
     * @private
     */
    requestInProcess_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the confirm button should be disabled.
     * @private
     */
    confirmButtonDisabled_: {
      type: Boolean,
      value: true,
    },

    /**
     * Authentication token provided by lock-screen-password-prompt-dialog.
     * @type {!chrome.quickUnlockPrivate.TokenInfo|undefined}
     */
    authToken: {
      type: Object,
      notify: true,
    },

    /**
     * Interface for chrome.quickUnlockPrivate calls. May be overridden by
     * tests.
     * @private
     */
    quickUnlockPrivate: {type: Object, value: chrome.quickUnlockPrivate},
  },

  observers: [
    'updateButtonState_(pinIncorrect_, requestInProcess_, pinValue_)',
  ],

  /** @override */
  attached() {
    this.resetState();
    this.$.dialog.showModal();
    this.$.pinKeyboard.focusInput();
  },

  close() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
    this.resetState();
  },

  resetState() {
    this.requestInProcess_ = false;
    this.pinValue_ = '';
    this.pinIncorrect_ = false;
  },

  /** @private */
  onCancelTap_() {
    this.close();
  },

  /**
   * Reset the PIN field after error.
   * @private
   */
  onPinChange_() {
    if (this.pinIncorrect_) {
      this.pinValue_ = '';
      this.pinIncorrect_ = false;
    }
  },

  /**
   * Make a request to the quick unlock API to enable PIN auto-submit.
   * @private
   */
  onPinSubmit_() {
    // Make a request to enable pin autosubmit.
    this.requestInProcess_ = true;
    this.quickUnlockPrivate.setPinAutosubmitEnabled(
        this.authToken.token, this.pinValue_ /* PIN */, true /*enabled*/,
        this.onPinSubmitResponse_.bind(this));
  },

  /**
   * Response from the quick unlock API.
   *
   * If the call is not successful because the PIN is incorrect,
   * it will check if its still possible to authenticate with PIN.
   * Submitting an invalid PIN will either show an error to the user,
   * or close the dialog and trigger a password re-prompt.
   * @param {Boolean} success
   * @private
   */
  onPinSubmitResponse_(success) {
    if (success) {
      this.close();
      return;
    }
    // Check if it is still possible to authenticate with pin.
    this.quickUnlockPrivate.canAuthenticatePin(
        this.onCanAuthenticateResponse_.bind(this));
  },

  /**
   * Response from the quick unlock API on whether PIN authentication
   * is currently possible.
   * @param {Boolean} can_authenticate
   */
  onCanAuthenticateResponse_(can_authenticate) {
    if (!can_authenticate) {
      this.fire('invalidate-auth-token-requested');
      this.close();
      return;
    }

    this.pinValue_ = '';
    this.requestInProcess_ = false;
    this.pinIncorrect_ = true;
    this.$.pinKeyboard.focusInput();
  },

  /** @private */
  updateButtonState_() {
    this.confirmButtonDisabled_ = this.requestInProcess_ ||
        this.pinIncorrect_ || (this.pinValue_ && this.pinValue_.length === 0);
  },
});
