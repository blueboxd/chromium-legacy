// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'gaia-password-changed',

  behaviors: [OobeI18nBehavior, LoginScreenBehavior],

  properties: {
    email: String,

    disabled: {type: Boolean, value: false},
  },

  /** @override */
  ready() {
    this.initializeLoginScreen('GaiaPasswordChangedScreen', {
      resetAllowed: false,
    });
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.PASSWORD_CHANGED;
  },

  // Invoked just before being shown. Contains all the data for the screen.
  onBeforeShow(data) {
    this.reset();
    this.email = data && 'email' in data && data.email;
    this.$.oldPasswordInput.invalid =
        data && 'showError' in data && data.showError;
  },

  reset() {
    this.$.animatedPages.selected = 0;
    this.clearPassword();
    this.disabled = false;
    this.$.navigation.closeVisible = true;
    this.$.oldPasswordCard.classList.remove('disabled');
  },

  focus() {
    if (this.$.animatedPages.selected == 0)
      this.$.oldPasswordInput.focus();
  },

  /** @private */
  onPasswordSubmitted_() {
    if (!this.$.oldPasswordInput.validate())
      return;
    this.$.oldPasswordCard.classList.add('disabled');
    this.disabled = true;
    chrome.send('migrateUserData', [this.$.oldPasswordInput.value]);
  },

  /** @private */
  onForgotPasswordClicked_() {
    this.clearPassword();
    this.$.animatedPages.selected += 1;
  },

  /** @private */
  onTryAgainClicked_() {
    this.$.oldPasswordInput.invalid = false;
    this.$.animatedPages.selected -= 1;
  },

  /** @private */
  onAnimationFinish_() {
    this.focus();
  },

  clearPassword() {
    this.$.oldPasswordInput.value = '';
    this.$.oldPasswordInput.invalid = false;
  },

  /** @private */
  onProceedClicked_() {
    this.disabled = true;
    this.$.navigation.closeVisible = false;
    this.$.animatedPages.selected = 2;
    chrome.send('login.GaiaPasswordChangedScreen.userActed', ['resync']);
  },

  /** @private */
  onClose_() {
    if (!this.disabled) {
      chrome.send('login.GaiaPasswordChangedScreen.userActed', ['cancel']);
    }
  }
});
