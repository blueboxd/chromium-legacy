// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'saml-confirm-password',

  behaviors: [OobeI18nBehavior],

  properties: {
    email: String,

    disabled: {type: Boolean, value: false, observer: 'disabledChanged_'},

    manualInput:
        {type: Boolean, value: false, observer: 'manualInputChanged_'}
  },

  ready() {
    /**
     * Workaround for
     * https://github.com/PolymerElements/neon-animation/issues/32
     * TODO(dzhioev): Remove when fixed in Polymer.
     */
    var pages = this.$.animatedPages;
    delete pages._squelchNextFinishEvent;
    Object.defineProperty(pages, '_squelchNextFinishEvent', {
      get() {
        return false;
      }
    });
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
    this.fire('cancel');
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
    this.fire('passwordEnter', {password: this.$.passwordInput.value});
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
  }
});
