// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordCheckListItem represents one leaked credential in the
 * list of compromised passwords.
 */

Polymer({
  is: 'password-check-list-item',

  properties: {
    /**
     * The password that is being displayed.
     * @type {!PasswordManagerProxy.CompromisedCredential}
     */
    item: Object,

    /** @private */
    isPasswordVisible_: {
      type: Boolean,
      computed: 'computePasswordVisibility_(item.password)',
    },

    /** @private */
    password_: {
      type: String,
      computed: 'computePassword_(item.password)',
    },
  },

  /**
   * @private {?PasswordManagerProxy}
   */
  passwordManager_: null,

  /** @override */
  attached() {
    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();
  },

  /**
   * @return {string}
   * @private
   */
  getCompromiseType_() {
    switch (this.item.compromiseType) {
      case chrome.passwordsPrivate.CompromiseType.PHISHED:
        return loadTimeData.getString('phishedPassword');
      case chrome.passwordsPrivate.CompromiseType.LEAKED:
        return loadTimeData.getString('leakedPassword');
      case chrome.passwordsPrivate.CompromiseType.PHISHED_AND_LEAKED:
        return loadTimeData.getString('phishedAndLeakedPassword');
    }

    assertNotReached(
        'Can\'t find a string for type: ' + this.item.compromiseType);
  },

  /**
   * @private
   */
  onChangePasswordClick_() {
    const url = assert(this.item.changePasswordUrl);
    settings.OpenWindowProxyImpl.getInstance().openURL(url);

    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordManagerProxy.PasswordCheckInteraction.CHANGE_PASSWORD);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMoreClick_(event) {
    this.fire('more-actions-click', {moreActionsButton: event.target});
  },

  /**
   * @return {string}
   * @private
   */
  getInputType_() {
    return this.isPasswordVisible_ ? 'text' : 'password';
  },

  /**
   * @return {boolean}
   * @private
   */
  computePasswordVisibility_() {
    return !!this.item.password;
  },

  /**
   * @return {string}
   * @private
   */
  computePassword_() {
    const NUM_PLACEHOLDERS = 10;
    return this.item.password || ' '.repeat(NUM_PLACEHOLDERS);
  },

  /**
   * @public
   */
  hidePassword() {
    this.set('item.password', null);
  },

  /**
   * @public
   */
  showPassword() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordManagerProxy.PasswordCheckInteraction.SHOW_PASSWORD);
    this.passwordManager_
        .getPlaintextCompromisedPassword(
            assert(this.item), chrome.passwordsPrivate.PlaintextReason.VIEW)
        .then(
            compromisedCredential => {
              this.set('item', compromisedCredential);
            },
            error => {
              this.hidePassword();
            });
  },

  /**
   * @private
   */
  onReadonlyInputTap_() {
    if (this.isPasswordVisible_) {
      this.$$('#leakedPassword').select();
    }
  },
});
