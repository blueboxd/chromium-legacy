// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network password input fields.
 */

// Used to indicate a saved but unknown credential value. Will appear as *'s in
// the credential (passphrase, password, etc.) field by default.
// See |kFakeCredential| in chromeos/network/policy_util.h.
/** @type {string} */ const FAKE_CREDENTIAL = 'FAKE_CREDENTIAL_VPaJDV9x';

Polymer({
  is: 'network-password-input',

  behaviors: [
    I18nBehavior,
    CrPolicyNetworkBehavior,
    NetworkConfigElementBehavior,
  ],

  properties: {
    label: {
      type: String,
      reflectToAttribute: true,
    },

    value: {
      type: String,
      notify: true,
    },

    showPassword: {
      type: Boolean,
      value: false,
    },

    showPolicyIndicator_: {
      type: Boolean,
      value: false,
      computed: 'getDisabled_(disabled, property)',
    },

    /** @private */
    restoreUnknown_: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'updateShowPassword_(value)',
  ],

  /** @private */
  updateShowPassword_: function() {
    if (this.value == FAKE_CREDENTIAL) {
      this.showPassword = false;
    }
  },

  focus: function() {
    this.$$('cr-input').focus();

    // If the input has any contents, the should be selected when focus is
    // applied.
    this.$$('cr-input').select();
  },

  /**
   * @return {string}
   * @private
   */
  getInputType_: function() {
    return this.showPassword ? 'text' : 'password';
  },

  /**
   * @return {string}
   * @private
   */
  getIconClass_: function() {
    return this.showPassword ? 'icon-visibility-off' : 'icon-visibility';
  },

  /**
   * @return {string}
   * @private
   */
  getShowPasswordTitle_: function() {
    return this.showPassword ? this.i18n('hidePassword') :
                               this.i18n('showPassword');
  },

  /**
   * @param {string}  value
   * @return {boolean} True if the value equals |FAKE_CREDENTIAL| to
   * prevent users from seeing this fake credential, but they should be able to
   * see their custom input.
   * @private
   */
  getButtonDisabled_: function(value) {
    return value == FAKE_CREDENTIAL;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onShowPasswordTap_: function(event) {
    this.showPassword = !this.showPassword;
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeypress_: function(event) {
    if (event.target.id != 'input' || event.key != 'Enter') {
      return;
    }
    event.stopPropagation();
    this.fire('enter');
  },

  /**
   * If the input field is focused and the value is |FAKE_CREDENTIAL|,
   * clear the value.
   * @param {!InputEvent} e
   * @private
   */
  onFocus_: function(e) {
    if (this.value != FAKE_CREDENTIAL) {
      return;
    }
    // We can not rely on data binding to update the target value when a
    // field is focused.
    e.target.value = '';
    this.value = '';
    // Remember to restore |FAKE_CREDENTIAL| if the user doesn't change
    // the input value.
    this.restoreUnknown_ = true;
  },

  /**
   * If the input field should be restored, restore the |FAKE_CREDENTIAL|
   * value when the field is unfocused.
   * @param {!InputEvent} e
   * @private
   */
  onBlur_: function(e) {
    if (!this.restoreUnknown_) {
      return;
    }
    // The target is still focused so we can not rely on data binding to
    // update the target value.
    e.target.value = FAKE_CREDENTIAL;
    this.value = FAKE_CREDENTIAL;
  },

  /**
   * When the input field is changed, clear |restoreUnknown_|.
   * @param {!InputEvent} e
   * @private
   */
  onInput_: function(e) {
    this.restoreUnknown_ = false;
  },
});
