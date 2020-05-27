// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Functions for Account manager screens.
 */
cr.define('account_manager', function() {
  /** @interface */
  class AccountManagerBrowserProxy {
    /**
     * Triggers the re-authentication flow for the account pointed to by
     * |account_email|.
     * @param {string} account_email
     */
    reauthenticateAccount(account_email) {}

    /**
     * Closes the dialog.
     */
    closeDialog() {}
  }

  /**
   * @implements {account_manager.AccountManagerBrowserProxy}
   */
  class AccountManagerBrowserProxyImpl {
    /** @override */
    reauthenticateAccount(account_email) {
      chrome.send('reauthenticateAccount', [account_email]);
    }

    /** @override */
    closeDialog() {
      chrome.send('closeDialog');
    }
  }

  cr.addSingletonGetter(AccountManagerBrowserProxyImpl);

  return {
    AccountManagerBrowserProxy: AccountManagerBrowserProxy,
    AccountManagerBrowserProxyImpl: AccountManagerBrowserProxyImpl,
  };
});
