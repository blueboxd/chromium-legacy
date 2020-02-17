// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordListItem represents one row in the list of passwords.
 * It needs to be its own component because FocusRowBehavior provides good a11y.
 */

Polymer({
  is: 'password-list-item',

  behaviors: [
    cr.ui.FocusRowBehavior,
    ShowPasswordBehavior,
  ],

  properties: {isOptedInForAccountStorage: {type: Boolean, value: false}},

  /**
   * Selects the password on tap if revealed.
   * @private
   */
  onReadonlyInputTap_() {
    if (this.item.password) {
      this.$$('#password').select();
    }
  },

  /**
   * Opens the password action menu.
   * @private
   */
  onPasswordMenuTap_() {
    this.fire(
        'password-menu-tap', {target: this.$.passwordMenu, listItem: this});
  },

  /**
   * @private
   * @param {!PasswordManagerProxy.UiEntryWithPassword} item This row's item.
   * @return {string}
   */
  getStorageText_(item) {
    // TODO(crbug.com/1049141): Add proper translated strings once we have them.
    return item.entry.fromAccountStore ? 'Account' : 'Local';
  },

  /**
   * @private
   * @param {!PasswordManagerProxy.UiEntryWithPassword} item This row's item.
   * @return {string}
   */
  getStorageIcon_(item) {
    // TODO(crbug.com/1049141): Add the proper icons once we know them.
    return item.entry.fromAccountStore ? 'cr:sync' : 'cr:computer';
  },

  /**
   * Get the aria label for the More Actions button on this row.
   * @param {!PasswordManagerProxy.UiEntryWithPassword} item This row's item.
   * @private
   */
  getMoreActionsLabel_(item) {
    // Avoid using I18nBehavior.i18n, because it will filter sequences, which
    // are otherwise not illegal for usernames. Polymer still protects against
    // XSS injection.
    return loadTimeData.getStringF(
        (item.entry.federationText) ? 'passwordRowFederatedMoreActionsButton' :
                                      'passwordRowMoreActionsButton',
        item.entry.username, item.entry.urls.shown);
  },
});
