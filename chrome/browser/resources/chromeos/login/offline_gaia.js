// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  const DEFAULT_EMAIL_DOMAIN = '@gmail.com';

  /** @enum */
  const TRANSITION_TYPE = {FORWARD: 0, BACKWARD: 1, NONE: 2};

  Polymer({
    is: 'offline-gaia',

    behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

    properties: {
      disabled: {
        type: Boolean,
        value: false,
      },

      /** @type {?string} */
      domain: {
        type: String,
        value: null,
      },

      /** @type {?string} */
      emailDomain: String,

      activeSection: {
        type: String,
        value: 'emailSection',
      },

      animationInProgress: Boolean,
    },

    attached() {
      if (this.isRTL_())
        this.setAttribute('rtl', '');
    },

    focus() {
      if (this.isEmailSectionActive_())
        this.$$('#emailInput').focus();
      else
        this.$$('#passwordInput').focus();
    },

    back() {
      this.switchToEmailCard(true /* animated */);
    },

    onBeforeShow() {
      this.behaviors.forEach((behavior) => {
        if (behavior.onBeforeShow)
          behavior.onBeforeShow.call(this);
      });
      this.$$('#dialog').onBeforeShow();
    },

    reset() {
      this.disabled = false;
      this.emailDomain = null;
      this.domain = null;
    },

    onForgotPasswordClicked_() {
      this.disabled = true;
      this.fire('dialogShown');
      this.$$('#forgotPasswordDlg').showModal();
    },

    onForgotPasswordCloseTap_() {
      this.$$('#forgotPasswordDlg').close();
    },

    onDialogOverlayClosed_() {
      this.fire('dialogHidden');
      this.disabled = false;
    },

    setEmail(email) {
      if (email) {
        if (this.emailDomain)
          email = email.replace(this.emailDomain, '');

        this.switchToPasswordCard(email, false /* animated */);
        this.$$('#passwordInput').isInvalid = true;
        this.fire('backButton', true);
      } else {
        this.$$('#emailInput').value = '';
        this.switchToEmailCard(false /* animated */);
      }
    },

    isRTL_() {
      return !!document.querySelector('html[dir=rtl]');
    },

    isEmailSectionActive_() {
      return this.activeSection == 'emailSection';
    },

    switchToEmailCard(animated) {
      this.$$('#passwordInput').value = '';
      this.$$('#passwordInput').isInvalid = false;
      this.$$('#emailInput').isInvalid = false;
      if (this.isEmailSectionActive_())
        return;

      this.animationInProgress = animated;
      this.activeSection = 'emailSection';
    },

    switchToPasswordCard(email, animated) {
      this.$$('#emailInput').value = email;
      if (email.indexOf('@') === -1) {
        if (this.emailDomain)
          email = email + this.emailDomain;
        else
          email = email + DEFAULT_EMAIL_DOMAIN;
      }
      this.$$('#passwordHeader').email = email;
      if (!this.isEmailSectionActive_())
        return;

      this.animationInProgress = animated;
      this.activeSection = 'passwordSection';
    },

    onSlideAnimationEnd_() {
      this.animationInProgress = false;
      this.focus();
    },

    onEmailSubmitted_() {
      if (this.$$('#emailInput').checkValidity()) {
        this.switchToPasswordCard(
            this.$$('#emailInput').value, true /* animated */);
      } else {
        this.$$('#emailInput').focus();
      }
    },

    onPasswordSubmitted_() {
      if (!this.$$('#passwordInput').checkValidity())
        return;
      var msg = {
        'useOffline': true,
        'email': this.$$('#passwordHeader').email,
        'password': this.$$('#passwordInput').value
      };
      this.$$('#passwordInput').value = '';
      this.fire('authCompleted', msg);
    },

    onBackButtonClicked_() {
      if (!this.isEmailSectionActive_()) {
        this.switchToEmailCard(true);
      } else {
        this.fire('offline-gaia-cancel');
      }
    },

    onNextButtonClicked_() {
      if (this.isEmailSectionActive_()) {
        this.onEmailSubmitted_();
        return;
      }
      this.onPasswordSubmitted_();
    },
  });
}
