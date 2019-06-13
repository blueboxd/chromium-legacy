// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Kerberos Accounts" subsection of
 * the "People" section of Settings, to interact with the browser. Chrome OS
 * only.
 */
cr.exportPath('settings');

/**
 * Information for a Chrome OS Kerberos account.
 * @typedef {{
 *   principalName: string,
 *   config: string,
 *   isSignedIn: boolean,
 *   isActive: boolean,
 *   hasRememberedPassword: boolean,
 *   pic: string,
 * }}
 */
settings.KerberosAccount;

cr.define('settings', function() {
  /**
   *  @enum {number}
   *  These values must be kept in sync with the ErrorType enum in
   *  third_party/cros_system_api/dbus/kerberos/kerberos_service.proto.
   */
  const KerberosErrorType = {
    kNone: 0,
    kUnknown: 1,
    kDBusFailure: 2,
    kNetworkProblem: 3,
    kUnknownKrb5Error: 4,
    kBadPrincipal: 5,
    kBadPassword: 6,
    kPasswordExpired: 7,
    kPasswordRejected: 8,
    kNoCredentialsCacheFound: 9,
    kKerberosTicketExpired: 10,
    kKdcDoesNotSupportEncryptionType: 11,
    kContactingKdcFailed: 12,
    kParseRequestFailed: 13,
    kLocalIo: 14,
    kUnknownPrincipalName: 15,
    kDuplicatePrincipalName: 16,
    kInProgress: 17,
    kParsePrincipalFailed: 18,
  };

  /** @interface */
  class KerberosAccountsBrowserProxy {
    /**
     * Returns a Promise for the list of Kerberos accounts held in the kerberosd
     * system daemon.
     * @return {!Promise<!Array<!settings.KerberosAccount>>}
     */
    getAccounts() {}

    /**
     * Attempts to add a new (or update an existing) Kerberos account.
     * @param {string} principalName Kerberos principal (user@realm.com).
     * @param {string} password Account password.
     * @param {boolean} rememberPassword Whether to store the password.
     * @param {string} config Kerberos configuration.
     * @param {boolean} allowExisting Whether existing accounts may be updated.
     * @return {!Promise<!settings.KerberosErrorType>}
     */
    addAccount(
        principalName, password, rememberPassword, config, allowExisting) {}

    /**
     * Removes |account| from the set of Kerberos accounts.
     * @param {!settings.KerberosAccount} account
     */
    removeAccount(account) {}

    /**
     * Sets |account| as currently active account. Kerberos credentials are
     * consumed from this account.
     * @param {!settings.KerberosAccount} account
     */
    setAsActiveAccount(account) {}
  }

  /**
   * @implements {settings.KerberosAccountsBrowserProxy}
   */
  class KerberosAccountsBrowserProxyImpl {
    /** @override */
    getAccounts() {
      return cr.sendWithPromise('getKerberosAccounts');
    }

    /** @override */
    addAccount(
        principalName, password, rememberPassword, config, allowExisting) {
      return cr.sendWithPromise(
          'addKerberosAccount', principalName, password, rememberPassword,
          config, allowExisting);
    }

    /** @override */
    removeAccount(account) {
      chrome.send('removeKerberosAccount', [account.principalName]);
    }

    /** @override */
    setAsActiveAccount(account) {
      chrome.send('setAsActiveKerberosAccount', [account.principalName]);
    }
  }

  cr.addSingletonGetter(KerberosAccountsBrowserProxyImpl);

  return {
    KerberosErrorType: KerberosErrorType,
    KerberosAccountsBrowserProxy: KerberosAccountsBrowserProxy,
    KerberosAccountsBrowserProxyImpl: KerberosAccountsBrowserProxyImpl,
  };
});
