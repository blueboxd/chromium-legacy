// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'saml-confirm-password',

  behaviors: [OobeI18nBehavior, LoginScreenBehavior],

  properties: {
    email: String,

    disabled: {type: Boolean, value: false, observer: 'disabledChanged_'},

    manualInput: {type: Boolean, value: false, observer: 'manualInputChanged_'}
  },

  EXTERNAL_API: ['show'],

  /**
   * Callback to run when the screen is dismissed.
   * @type {?function(string)}
   */
  callback_: null,

  ready() {
    this.initializeLoginScreen('ConfirmSamlPasswordScreen', {
      resetAllowed: true,
    });
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.SAML_PASSWORD_CONFIRM;
  },

  onAfterShow(data) {
    this.focus();
  },

  onBeforeHide() {
    this.reset();
  },

  /**
   * Shows the confirm password screen.
   * @param {string} email The authenticated user's e-mail.
   * @param {boolean} manualPasswordInput True if no password has been
   *     scrapped and the user needs to set one manually for the device.
   * @param {number} attemptCount Number of attempts tried, starting at 0.
   * @param {function(string)} callback The callback to be invoked when the
   *     screen is dismissed.
   */
  show(email, manualPasswordInput, attemptCount, callback) {
    this.callback_ = callback;
    this.reset();
    this.email = email;
    this.manualInput = manualPasswordInput;
    if (attemptCount > 0)
      this.$.passwordInput.invalid = true;
    cr.ui.Oobe.showScreen({id: 'saml-confirm-password'});
  },

  reset() {
    if (this.$.cancelConfirmDlg.open)
      this.$.cancelConfirmDlg.close();
    this.disabled = false;
    this.$.navigation.closeVisible = true;
    if (this.$.animatedPages.selected != 0)
      this.$.animatedPages.selected = 0;
    this.$.passwordInput.invalid = false;
    this.$.passwordInput.value = '';
    if (this.manualInput) {
      this.$$('#confirmPasswordInput').invalid = false;
      this.$$('#confirmPasswordInput').value = '';
    }
  },

  invalidate() {
    this.$.passwordInput.invalid = true;
  },

  focus() {
    if (this.$.animatedPages.selected == 0)
      this.$.passwordInput.focus();
  },

  onClose_() {
    this.disabled = true;
    this.$.cancelConfirmDlg.showModal();
  },

  onCancelNo_() {
    this.$.cancelConfirmDlg.close();
  },

  onCancelYes_() {
    this.$.cancelConfirmDlg.close();

    cr.ui.Oobe.showScreen({id: 'gaia-signin'});
    cr.ui.Oobe.resetSigninUI(true);
  },

  onPasswordSubmitted_() {
    if (!this.$.passwordInput.validate())
      return;
    if (this.manualInput) {
      // When using manual password entry, both passwords must match.
      var confirmPasswordInput = this.$$('#confirmPasswordInput');
      if (!confirmPasswordInput.validate())
        return;

      if (confirmPasswordInput.value != this.$.passwordInput.value) {
        this.$.passwordInput.invalid = true;
        confirmPasswordInput.invalid = true;
        return;
      }
    }

    this.$.animatedPages.selected = 1;
    this.$.navigation.closeVisible = false;

    this.callback_(this.$.passwordInput.value);
  },

  onDialogOverlayClosed_() {
    this.disabled = false;
  },

  disabledChanged_(disabled) {
    this.$.confirmPasswordCard.classList.toggle('full-disabled', disabled);
  },

  onAnimationFinish_() {
    if (this.$.animatedPages.selected == 1)
      this.$.passwordInput.value = '';
  },

  manualInputChanged_() {
    var titleId =
        this.manualInput ? 'manualPasswordTitle' : 'confirmPasswordTitle';
    var passwordInputLabelId =
        this.manualInput ? 'manualPasswordInputLabel' : 'confirmPasswordLabel';
    var passwordInputErrorId = this.manualInput ?
        'manualPasswordMismatch' :
        'confirmPasswordIncorrectPassword';

    this.$.title.textContent = loadTimeData.getString(titleId);
    this.$.passwordInput.label = loadTimeData.getString(passwordInputLabelId);
    this.$.passwordInput.error = loadTimeData.getString(passwordInputErrorId);
  },

  getConfirmPasswordInputLabel_() {
    return loadTimeData.getString('confirmPasswordLabel');
  },

  getConfirmPasswordInputError_() {
    return loadTimeData.getString('manualPasswordMismatch');
  },
});
