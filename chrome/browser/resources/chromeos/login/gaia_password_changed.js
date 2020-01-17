// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'gaia-password-changed',

  behaviors: [OobeI18nBehavior],

  properties: {
    email: String,

    disabled: {type: Boolean, value: false}
  },

  /** @override */
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

  invalidate() {
    this.$.oldPasswordInput.invalid = true;
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
    this.fire('passwordEnter', {password: this.$.oldPasswordInput.value});
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
    this.fire('proceedAnyway');
  },

  /** @private */
  onClose_() {
    this.fire('cancel');
  }
});
