// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-check-edit-dialog' is the dialog that allows showing
 * a saved password.
 */

(function() {

Polymer({
  is: 'settings-password-check-edit-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The password that the user is interacting with now.
     * @type {?PasswordManagerProxy.CompromisedCredential}
     */
    item: Object,

    /**
     * Whether the password is visible or obfuscated.
     * @private
     */
    visible: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?PasswordManagerProxy} */
  passwordManager_: null,

  /** @override */
  attached() {
    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();
    this.$.dialog.showModal();
    cr.ui.focusWithoutInk(this.$.cancel);
  },

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  },

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancel_() {
    this.close();
  },

  /**
   * Handler for tapping the 'save' button. Should just dismiss the dialog.
   * @private
   */
  onSave_() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordManagerProxy.PasswordCheckInteraction.EDIT_PASSWORD);
    this.passwordManager_
        .changeCompromisedCredential(
            assert(this.item), this.$.passwordInput.value)
        .then(() => {
          this.close();
        });
  },

  /**
   * @private
   * @return {string} The title text for the show/hide icon.
   */
  showPasswordTitle_() {
    return this.i18n(this.visible ? 'hidePassword' : 'showPassword');
  },

  /**
   * @private
   * @return {string} The visibility icon class, depending on whether the
   *     password is already visible.
   */
  showPasswordIcon_() {
    return this.visible ? 'icon-visibility-off' : 'icon-visibility';
  },

  /**
   * @private
   * @return {string} The type of the password input field (text or password),
   *     depending on whether the password should be obfuscated.
   */
  getPasswordInputType_() {
    return this.visible ? 'text' : 'password';
  },

  /**
   * Handler for tapping the show/hide button.
   * @private
   */
  onShowPasswordButtonClick_() {
    this.visible = !this.visible;
  },

  /**
   * @private
   * @return {string} The text to be displayed as the dialog's footnote.
   */
  getFootnote_() {
    return this.i18n(
        'editCompromisedPasswordFootnote', this.item.formattedOrigin);
  },

  /**
   * @private
   * @return {string} The label for the origin, depending on the whether it's a
   *     site or an app.
   */
  getSiteOrApp_() {
    return this.i18n(
        this.item.isAndroidCredential ? 'editCompromisedPasswordApp' :
                                        'editCompromisedPasswordSite');
  }
});
})();
