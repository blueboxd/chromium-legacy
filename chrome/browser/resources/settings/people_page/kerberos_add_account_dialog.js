// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'kerberos-add-account-dialog' is an element to add Kerberos accounts.
 */

Polymer({
  is: 'kerberos-add-account-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * If set, some fields are preset from this account (like username or
     * whether to remember the password).
     * @type {?settings.KerberosAccount}
     */
    presetAccount: Object,

    /** @private */
    username_: {
      type: String,
      value: '',
    },

    /** @private */
    password_: {
      type: String,
      value: '',
    },

    /**
     * Current configuration in the Advanced Config dialog. Propagates to
     * |config| only if 'Save' button is pressed.
     * @private {string}
     */
    editableConfig_: {
      type: String,
      value: '',
    },

    /** @private */
    rememberPassword_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    generalErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    usernameErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    passwordErrorText_: {
      type: String,
      value: '',
    },

    /** @private */
    inProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showAdvancedConfig_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {boolean} */
  useRememberedPassword_: false,

  /** @private {string} */
  config_: '',

  /** @override */
  attached: function() {
    this.$.addDialog.showModal();

    if (this.presetAccount) {
      // Preset username and make UI read-only.
      // Note: At least the focus() part needs to be after showModal.
      this.username_ = this.presetAccount.principalName;
      this.$.username.readonly = true;
      this.$.password.focus();

      if (this.presetAccount.hasRememberedPassword) {
        // The daemon knows the user's password, so prefill the password field
        // with some string (Chrome does not know the actual password for
        // security reasons). If the user does not change it, an empty password
        // is sent to the daemon, which is interpreted as "use remembered
        // password". Also, keep remembering the password by default.
        const FAKE_PASSWORD = 'xxxxxxxx';
        this.password_ = FAKE_PASSWORD;
        this.rememberPassword_ = true;
        this.useRememberedPassword_ = true;
      }

      this.config_ = this.presetAccount.config;
    } else {
      // Set a default configuration.
      this.config_ = loadTimeData.getString('defaultKerberosConfig');
    }
  },

  /** @private */
  onCancel_: function() {
    this.$.addDialog.cancel();
  },

  /** @private */
  onAdd_: function() {
    this.inProgress_ = true;
    this.updateErrorMessages_(settings.KerberosErrorType.kNone);

    // An empty password triggers the Kerberos daemon to use the remembered one.
    const passwordToSubmit = this.useRememberedPassword_ ? '' : this.password_;

    // For new accounts (no preset), bail if the account already exists.
    const allowExisting = !!this.presetAccount;

    settings.KerberosAccountsBrowserProxyImpl.getInstance()
        .addAccount(
            this.username_, passwordToSubmit, this.rememberPassword_,
            this.config_, allowExisting)
        .then(error => {
          this.inProgress_ = false;

          // Success case. Close dialog.
          if (error == settings.KerberosErrorType.kNone) {
            this.$.addDialog.close();
            return;
          }

          // Triggers the UI to update error messages.
          this.updateErrorMessages_(error);
        });
  },

  /** @private */
  onPasswordInput_: function() {
    // On first input, don't reuse the remembered password, but submit the
    // changed one.
    this.useRememberedPassword_ = false;
  },

  /** @private */
  onAdvancedConfigClick_: function() {
    // Keep a copy of the config in case the user cancels.
    this.editableConfig_ = this.config_;
    this.showAdvancedConfig_ = true;
    Polymer.dom.flush();
    this.$$('#advancedConfigDialog').showModal();
  },

  /** @private */
  onAdvancedConfigCancel_: function() {
    this.showAdvancedConfig_ = false;
    this.$$('#advancedConfigDialog').cancel();
  },

  /** @private */
  onAdvancedConfigSave_: function() {
    this.showAdvancedConfig_ = false;
    this.config_ = this.editableConfig_;
    this.$$('#advancedConfigDialog').close();
  },

  onAdvancedConfigClose_: function(event) {
    // Note: 'Esc' doesn't trigger onAdvancedConfigCancel_() and some tests
    // that trigger onAdvancedConfigCancel_() don't trigger this for some
    // reason, hence this is needed here and above.
    this.showAdvancedConfig_ = false;

    // Since this is a sub-dialog, prevent event from bubbling up. Otherwise,
    // it might cause the add-dialog to be closed.
    event.stopPropagation();
  },

  /**
   * @param {!settings.KerberosErrorType} error Current error enum
   * @private
   */
  updateErrorMessages_: function(error) {
    this.generalErrorText_ = '';
    this.usernameErrorText_ = '';
    this.passwordErrorText_ = '';

    switch (error) {
      case settings.KerberosErrorType.kNone:
        break;

      case settings.KerberosErrorType.kNetworkProblem:
        this.generalErrorText_ = this.i18n('kerberosErrorNetworkProblem');
        break;
      case settings.KerberosErrorType.kParsePrincipalFailed:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameInvalid');
        break;
      case settings.KerberosErrorType.kBadPrincipal:
        this.usernameErrorText_ = this.i18n('kerberosErrorUsernameUnknown');
        break;
      case settings.KerberosErrorType.kDuplicatePrincipalName:
        this.usernameErrorText_ =
            this.i18n('kerberosErrorDuplicatePrincipalName');
        break;
      case settings.KerberosErrorType.kContactingKdcFailed:
        this.usernameErrorText_ = this.i18n('kerberosErrorContactingServer');
        break;

      case settings.KerberosErrorType.kBadPassword:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordInvalid');
        break;
      case settings.KerberosErrorType.kPasswordExpired:
        this.passwordErrorText_ = this.i18n('kerberosErrorPasswordExpired');
        break;

      case settings.KerberosErrorType.kKdcDoesNotSupportEncryptionType:
        this.generalErrorText_ = this.i18n('kerberosErrorKdcEncType');
        break;
      default:
        this.generalErrorText_ =
            this.i18n('kerberosErrorGeneral', String(error));
    }
  },

  /**
   * Whether an error element should be shown.
   * Note that !! is not supported in Polymer bindings.
   * @param {?string} errorText Error text to be displayed. Empty if no error.
   * @return {boolean} True iff errorText is not empty.
   * @private
   */
  showError_: function(errorText) {
    return !!errorText;
  }
});