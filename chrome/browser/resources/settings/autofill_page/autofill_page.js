// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-autofill-page' is the settings page containing settings for
 * passwords, payment methods and addresses.
 */
Polymer({
  is: 'settings-autofill-page',

  behaviors: [PrefsBehavior],

  properties: {
    /** @private Filter applied to passwords and password exceptions. */
    passwordFilter_: String,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.PASSWORDS) {
          map.set(settings.routes.PASSWORDS.path, '#passwordManagerButton');
        }
        if (settings.routes.PAYMENTS) {
          map.set(settings.routes.PAYMENTS.path, '#paymentManagerButton');
        }
        if (settings.routes.ADDRESSES) {
          map.set(settings.routes.ADDRESSES.path, '#addressesManagerButton');
        }

        return map;
      },
    },
  },

  /**
   * Shows the manage addresses sub page.
   * @param {!Event} event
   * @private
   */
  onAddressesClick_(event) {
    settings.Router.getInstance().navigateTo(settings.routes.ADDRESSES);
  },

  /**
   * Shows the manage payment methods sub page.
   * @private
   */
  onPaymentsClick_() {
    settings.Router.getInstance().navigateTo(settings.routes.PAYMENTS);
  },

  /**
   * Shows a page to manage passwords. This is either the passwords sub page or
   * the Google Password Manager page.
   * @private
   */
  onPasswordsClick_() {
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    loadTimeData.getBoolean('navigateToGooglePasswordManager') ?
        settings.OpenWindowProxyImpl.getInstance().openURL(
            loadTimeData.getString('googlePasswordManagerUrl')) :
        settings.Router.getInstance().navigateTo(settings.routes.PASSWORDS);
  },
});
