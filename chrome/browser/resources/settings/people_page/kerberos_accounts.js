// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-accounts' is the settings subpage containing controls to
 * list, add and delete Kerberos Accounts.
 */

'use strict';

Polymer({
  is: 'settings-kerberos-accounts',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of Accounts.
     * @private {!Array<!settings.KerberosAccount>}
     */
    accounts_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The targeted account for menu and other operations.
     * @private {?settings.KerberosAccount}
     */
    selectedAccount_: Object,

    /** @private */
    showAddAccountDialog_: Boolean,
  },

  /** @private {?settings.KerberosAccountsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'kerberos-accounts-changed', this.refreshAccounts_.bind(this));
  },

  /** @override */
  ready: function() {
    this.browserProxy_ =
        settings.KerberosAccountsBrowserProxyImpl.getInstance();
    this.refreshAccounts_();
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_: function(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAddAccountClick_: function(event) {
    this.selectedAccount_ = null;
    this.showAddAccountDialog_ = true;
  },

  /**
   * @param {!CustomEvent<!{model: !{item: !settings.Account}}>} event
   * @private
   */
  onReauthenticationClick_: function(event) {
    this.selectedAccount_ = event.model.item;
    this.showAddAccountDialog_ = true;
  },

  /** @private */
  onAddAccountDialogClosed_: function() {
    this.showAddAccountDialog_ = false;
    // In case it was opened by the 'Refresh now' action menu.
    this.closeActionMenu_();
  },

  /** @private */
  refreshAccounts_: function() {
    this.browserProxy_.getAccounts().then(accounts => {
      this.accounts_ = accounts;
    });
  },

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: !settings.KerberosAccount}, target: !Element}}
   *      event
   * @private
   */
  onAccountActionsMenuButtonClick_: function(event) {
    this.selectedAccount_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(event.target);
  },

  /**
   * Closes action menu and resets action menu model.
   * @private
   */
  closeActionMenu_: function() {
    this.$$('cr-action-menu').close();
    this.selectedAccount_ = null;
  },

  /**
   * Removes |this.selectedAccount_|.
   * @private
   */
  onRemoveAccountClick_: function() {
    this.browserProxy_.removeAccount(
        /** @type {!settings.KerberosAccount} */ (this.selectedAccount_));
    this.closeActionMenu_();
  },

  /**
   * Sets |this.selectedAccount_| as active Kerberos account.
   * @private
   */
  onSetAsActiveAccountClick_: function() {
    this.browserProxy_.setAsActiveAccount(
        /** @type {!settings.KerberosAccount} */ (this.selectedAccount_));
    this.closeActionMenu_();
  },

  /**
   * Opens the reauth dialog for |this.selectedAccount_|.
   * @private
   */
  onRefreshNowClick_: function() {
    this.showAddAccountDialog_ = true;
  },

  /**
   * @param {boolean} isActive Whether a Kerberos account is active.
   * @return {string} Localized label for the active/inactive state.
   * @private
   */
  getActiveLabel_: function(isActive) {
    // TODO(https://crbug.com/969850): Localize.
    return isActive ? ', active' : '';
  }
});
