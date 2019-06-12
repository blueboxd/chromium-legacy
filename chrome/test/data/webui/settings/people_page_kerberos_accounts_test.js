// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

cr.define('settings_people_page_kerberos_accounts', function() {
  /** @implements {settings.KerberosAccountsBrowserProxy} */
  class TestKerberosAccountsBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'getAccounts',
        'addAccount',
        'removeAccount',
        'setAsActiveAccount',
      ]);

      // Simulated error from a addKerberosAccount call.
      this.addAccountError = settings.KerberosErrorType.kNone;
    }

    /** @override */
    getAccounts() {
      this.methodCalled('getAccounts');

      return Promise.resolve([
        {
          principalName: 'user@REALM',
          isSignedIn: true,
          isActive: true,
          pic: 'pic',
        },
        {
          principalName: 'user2@REALM2',
          isSignedIn: false,
          isActive: false,
          pic: 'pic2',
        }
      ]);
    }

    /** @override */
    addAccount(principalName, password) {
      this.methodCalled('addAccount', [principalName, password]);
      return Promise.resolve(this.addAccountError);
    }

    /** @override */
    removeAccount(account) {
      this.methodCalled('removeAccount', account);
    }

    /** @override */
    setAsActiveAccount(account) {
      this.methodCalled('setAsActiveAccount', account);
    }
  }

  // Tests for the Kerberos Accounts settings page.
  suite('KerberosAccountsTests', function() {
    let browserProxy = null;
    let kerberosAccounts = null;
    let accountList = null;

    // Account indices (to help readability).
    const Account = {
      FIRST: 0,
      SECOND: 1,
    };

    // Indices of 'More Actions' buttons.
    const MoreActions = {
      SET_AS_ACTIVE_ACCOUNT: 0,
      REMOVE_ACCOUNT: 1,
    };

    setup(function() {
      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();

      kerberosAccounts = document.createElement('settings-kerberos-accounts');
      document.body.appendChild(kerberosAccounts);
      accountList = kerberosAccounts.$$('#account-list');
      assertTrue(!!accountList);

      settings.navigateTo(settings.routes.KERBEROS_ACCOUNTS);
    });

    teardown(function() {
      kerberosAccounts.remove();
    });

    function clickMoreActions(accountIndex, moreActionsIndex) {
      // Click 'More actions' for the given account.
      kerberosAccounts.root.querySelectorAll('cr-icon-button')[accountIndex]
          .click();
      // Click on the given action.
      kerberosAccounts.$$('cr-action-menu')
          .querySelectorAll('button')[moreActionsIndex]
          .click();
    }

    test('AccountListIsPopulatedAtStartup', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // 2 accounts were added in |getAccounts()| mock above.
        assertEquals(2, accountList.items.length);
      });
    });

    test('AddAccount', function() {
      assertTrue(!kerberosAccounts.$$('kerberos-add-account-dialog'));
      assertFalse(kerberosAccounts.$$('#add-account-button').disabled);
      kerberosAccounts.$$('#add-account-button').click();
      Polymer.dom.flush();
      const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
      assertTrue(!!addDialog);
      assertEquals('', addDialog.username);
    });

    test('ReauthenticateAccount', function() {
      // Wait until accounts are loaded.
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();

        // The kerberos-add-account-dialog shouldn't be open yet.
        assertTrue(!kerberosAccounts.$$('kerberos-add-account-dialog'));

        // Click "Sign-In" on an existing account.
        // Note that both accounts have a reauth button, but [0] is hidden, so
        // click [1] (clicking a hidden button works, but it feels weird).
        kerberosAccounts.root.querySelectorAll('.reauth-button')[1].click();
        Polymer.dom.flush();

        // Now the kerberos-add-account-dialog should be open with preset
        // username.
        const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
        assertTrue(!!addDialog);
        assertEquals('user2@REALM2', addDialog.username);
      });
    });

    test('RemoveAccount', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        clickMoreActions(Account.FIRST, MoreActions.REMOVE_ACCOUNT);
        return browserProxy.whenCalled('removeAccount').then((account) => {
          assertEquals('user@REALM', account.principalName);
        });
      });
    });

    test('AccountListIsUpdatedWhenKerberosAccountsUpdates', function() {
      assertEquals(1, browserProxy.getCallCount('getAccounts'));
      cr.webUIListenerCallback('kerberos-accounts-changed');
      assertEquals(2, browserProxy.getCallCount('getAccounts'));
    });

    test('SetAsActiveAccount', function() {
      return browserProxy.whenCalled('getAccounts')
          .then(() => {
            Polymer.dom.flush();
            clickMoreActions(Account.SECOND, MoreActions.SET_AS_ACTIVE_ACCOUNT);
            return browserProxy.whenCalled('setAsActiveAccount');
          })
          .then((account) => {
            assertEquals('user2@REALM2', account.principalName);
          });
    });
  });

  // Tests for the kerberos-add-account-dialog element.
  suite('KerberosAddAccountTests', function() {
    let browserProxy = null;
    let dialog = null;
    let username = null;
    let password = null;
    let addButton = null;
    let generalError = null;

    setup(function() {
      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();

      dialog = document.createElement('kerberos-add-account-dialog');
      document.body.appendChild(dialog);

      username = dialog.$.username;
      assertTrue(!!username);

      password = dialog.$.password;
      assertTrue(!!password);

      addButton = dialog.$$('.action-button');
      assertTrue(!!addButton);

      generalError = dialog.$['general-error-message'];
      assertTrue(!!generalError);
    });

    teardown(function() {
      dialog.remove();
    });

    // Sets |error| as error result for addAccount(), simulates a click on the
    // addAccount button and checks that |errorElement| has an non-empty
    // innerText value afterwards.
    function checkAddAccountError(error, errorElement) {
      Polymer.dom.flush();
      assertEquals(0, errorElement.innerText.length);
      browserProxy.addAccountError = error;
      addButton.click();
      return browserProxy.whenCalled('addAccount').then(function() {
        Polymer.dom.flush();
        assertNotEquals(0, errorElement.innerText.length);
      });
    }

    // The username input field is not disabled by default.
    test('UsernameFieldNotDisabledByDefault', function() {
      assertFalse(username.disabled);
    });

    // The username input field is disabled if a username is preset before the
    // dialog is appended to the document.
    test('UsernameFieldDisabledIfPreset', function() {
      const newDialog = document.createElement('kerberos-add-account-dialog');
      newDialog.username = 'user';
      document.body.appendChild(newDialog);
      assertTrue(newDialog.$.username.disabled);
    });

    // By clicking the "Add account", the username and password values are
    // passed to the 'addAccount' browser proxy method.
    test('AddButtonPassesCredentials', function() {
      const EXPECTED_USER = 'testuser';
      const EXPECTED_PASS = 'testpass';
      username.value = EXPECTED_USER;
      password.value = EXPECTED_PASS;
      assertFalse(addButton.disabled);
      addButton.click();
      return browserProxy.whenCalled('addAccount').then(function(args) {
        assertEquals(EXPECTED_USER, args[0]);
        assertEquals(EXPECTED_PASS, args[1]);
      });
    });

    // While an account is being added, the "Add account" is disabled.
    test('AddButtonDisableWhileInProgress', function() {
      assertFalse(addButton.disabled);
      addButton.click();
      assertTrue(addButton.disabled);
      return browserProxy.whenCalled('addAccount').then(function(args) {
        assertFalse(addButton.disabled);
      });
    });

    // addAccount: KerberosErrorType.kNetworkProblem spawns a general error.
    test('AddAccountError_NetworkProblem', function() {
      checkAddAccountError(
          settings.KerberosErrorType.kNetworkProblem, generalError);
    });

    // addAccount: KerberosErrorType.kParsePrincipalFailed spawns a username
    // error.
    test('AddAccountError_ParsePrincipalFailed', function() {
      checkAddAccountError(
          settings.KerberosErrorType.kParsePrincipalFailed, username.$.error);
    });

    // addAccount: KerberosErrorType.kBadPrincipal spawns a username error.
    test('AddAccountError_BadPrincipal', function() {
      checkAddAccountError(
          settings.KerberosErrorType.kBadPrincipal, username.$.error);
    });

    // addAccount: KerberosErrorType.kContactingKdcFailed spawns a username
    // error.
    test('AddAccountError_ContactingKdcFailed', function() {
      checkAddAccountError(
          settings.KerberosErrorType.kContactingKdcFailed, username.$.error);
    });

    // addAccount: KerberosErrorType.kBadPassword spawns a password error.
    test('AddAccountError_BadPassword', function() {
      checkAddAccountError(
          settings.KerberosErrorType.kBadPassword, password.$.error);
    });

    // addAccount: KerberosErrorType.kPasswordExpired spawns a password error.
    test('AddAccountError_PasswordExpired', function() {
      checkAddAccountError(
          settings.KerberosErrorType.kPasswordExpired, password.$.error);
    });

    // addAccount: KerberosErrorType.kKdcDoesNotSupportEncryptionType spawns a
    // general error.
    test('AddAccountError_KdcDoesNotSupportEncryptionType', function() {
      checkAddAccountError(
          settings.KerberosErrorType.kKdcDoesNotSupportEncryptionType,
          generalError);
    });

    // addAccount: KerberosErrorType.kUnknown spawns a general error.
    test('AddAccountError_Unknown', function() {
      checkAddAccountError(settings.KerberosErrorType.kUnknown, generalError);
    });
  });
});