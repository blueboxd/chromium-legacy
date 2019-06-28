// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

cr.define('settings_people_page_kerberos_accounts', function() {
  // List of fake accounts.
  const testAccounts = [
    {
      principalName: 'user@REALM',
      config: 'config1',
      isSignedIn: true,
      isActive: true,
      isManaged: false,
      passwordWasRemembered: false,
      pic: 'pic',
    },
    {
      principalName: 'user2@REALM2',
      config: 'config2',
      isSignedIn: false,
      isActive: false,
      isManaged: false,
      passwordWasRemembered: true,
      pic: 'pic2',
    },
    {
      principalName: 'user3@REALM3',
      config: 'config3',
      isSignedIn: false,
      isActive: false,
      isManaged: true,
      passwordWasRemembered: true,
      pic: 'pic2',
    }
  ];

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
      return Promise.resolve(testAccounts);
    }

    /** @override */
    addAccount(
        principalName, password, rememberPassword, config, allowExisting) {
      this.methodCalled(
          'addAccount',
          [principalName, password, rememberPassword, config, allowExisting]);
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
      THIRD: 1,
    };

    // Indices of 'More Actions' buttons.
    const MoreActions = {
      REFRESH_NOW: 0,
      SET_AS_ACTIVE_ACCOUNT: 1,
      REMOVE_ACCOUNT: 2,
    };

    setup(function() {
      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      createDialog();
    });

    teardown(function() {
      kerberosAccounts.remove();
    });

    function createDialog() {
      if (kerberosAccounts) {
        kerberosAccounts.remove();
      }

      kerberosAccounts = document.createElement('settings-kerberos-accounts');
      document.body.appendChild(kerberosAccounts);

      accountList = kerberosAccounts.$$('#account-list');
      assertTrue(!!accountList);
    }

    function clickMoreActions(accountIndex, moreActionsIndex) {
      // Click 'More actions' for the given account.
      kerberosAccounts.shadowRoot
          .querySelectorAll('.more-actions')[accountIndex]
          .click();
      // Click on the given action.
      kerberosAccounts.$$('cr-action-menu')
          .querySelectorAll('button')[moreActionsIndex]
          .click();
    }

    test('AccountListIsPopulatedAtStartup', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        // The test accounts were added in |getAccounts()| mock above.
        assertEquals(testAccounts.length, accountList.items.length);
      });
    });

    test('AddAccount', function() {
      assertTrue(!kerberosAccounts.$$('kerberos-add-account-dialog'));
      kerberosAccounts.$$('#add-account-button').click();
      Polymer.dom.flush();
      const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
      assertTrue(!!addDialog);
      assertEquals('', addDialog.$.username.value);
    });

    test('ReauthenticateAccount', function() {
      // Wait until accounts are loaded.
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();

        // The kerberos-add-account-dialog shouldn't be open yet.
        assertTrue(!kerberosAccounts.$$('kerberos-add-account-dialog'));

        // Click "Sign-In" on an existing account.
        // Note that both accounts have a reauth button, but the first one is
        // hidden, so click the second one (clicking a hidden button works, but
        // it feels weird).
        kerberosAccounts.shadowRoot
            .querySelectorAll('.reauth-button')[Account.SECOND]
            .click();
        Polymer.dom.flush();

        // Now the kerberos-add-account-dialog should be open with preset
        // username.
        const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
        assertTrue(!!addDialog);
        assertEquals(
            testAccounts[Account.SECOND].principalName,
            addDialog.$.username.value);
      });
    });

    // Appending '?kerberos_reauth=<principal>' to the URL opens the reauth
    // dialog for that account.
    test('HandleReauthQueryParameter', function(done) {
      const principal_name = testAccounts[Account.FIRST].principalName;
      const params = new URLSearchParams;
      params.append('kerberos_reauth', principal_name);
      settings.navigateTo(settings.routes.KERBEROS_ACCOUNTS, params);

      // The setTimeout is necessary since the kerberos_reauth param would
      // otherwise be handled AFTER the callback below is executed.
      browserProxy.whenCalled('getAccounts').then(() => {
        setTimeout(() => {
          Polymer.dom.flush();
          const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
          assertTrue(!!addDialog);
          assertEquals(principal_name, addDialog.$.username.value);
          done();
        });
      });
    });

    test('RefreshNow', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        clickMoreActions(Account.FIRST, MoreActions.REFRESH_NOW);
        Polymer.dom.flush();

        const addDialog = kerberosAccounts.$$('kerberos-add-account-dialog');
        assertTrue(!!addDialog);
        assertEquals(
            testAccounts[Account.FIRST].principalName,
            addDialog.$.username.value);
      });
    });

    test('RemoveAccount', function() {
      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        clickMoreActions(Account.FIRST, MoreActions.REMOVE_ACCOUNT);
        return browserProxy.whenCalled('removeAccount').then((account) => {
          assertEquals(
              testAccounts[Account.FIRST].principalName, account.principalName);
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
            assertEquals(
                testAccounts[Account.SECOND].principalName,
                account.principalName);
          });
    });

    test('ShowPolicyIndicatorForManagedAccounts', function() {
      // Make sure we have at least one managed and one unmanaged account.
      assertFalse(testAccounts[0].isManaged);
      assertTrue(testAccounts[2].isManaged);

      return browserProxy.whenCalled('getAccounts').then(() => {
        Polymer.dom.flush();
        accountList =
            kerberosAccounts.shadowRoot.querySelectorAll('.account-list-item');
        assertEquals(testAccounts.length, accountList.length);

        for (let i = 0; i < testAccounts.length; i++) {
          // Assert account has policy indicator iff account is managed.
          const hasPolicyIndicator =
              !!accountList[i].querySelector('.account-policy-indicator');
          assertEquals(testAccounts[i].isManaged, hasPolicyIndicator);

          // Assert 'Remove' button is disabled iff account is managed.
          accountList[i].querySelector('.more-actions').click();
          const moreActions =
              kerberosAccounts.$$('cr-action-menu').querySelectorAll('button');
          const removeAccountDisabled =
              moreActions[MoreActions.REMOVE_ACCOUNT].disabled;
          assertEquals(testAccounts[i].isManaged, removeAccountDisabled);
          kerberosAccounts.$$('cr-action-menu').close();
        }
      });
    });

    test('AddAccountsAllowed', function() {
      assertTrue(loadTimeData.getBoolean('kerberosAddAccountsAllowed'));
      createDialog();
      assertTrue(!kerberosAccounts.$$('#add-account-policy-indicator'));
      assertFalse(kerberosAccounts.$$('#add-account-button').disabled);
    });

    test('AddAccountsNotAllowed', function() {
      loadTimeData.overrideValues({kerberosAddAccountsAllowed: false});
      createDialog();
      Polymer.dom.flush();
      assertTrue(!!kerberosAccounts.$$('#add-account-policy-indicator'));
      assertTrue(kerberosAccounts.$$('#add-account-button').disabled);

      // Reset for further tests.
      loadTimeData.overrideValues({kerberosAddAccountsAllowed: true});
    });
  });

  // Tests for the kerberos-add-account-dialog element.
  suite('KerberosAddAccountTests', function() {
    let browserProxy = null;

    let dialog = null;
    let addDialog = null;

    let username = null;
    let password = null;
    let rememberPassword = null;
    let advancedConfigButton = null;
    let addButton = null;
    let generalError = null;

    // Indices of 'addAccount' params.
    const AddParams = {
      PRINCIPAL_NAME: 0,
      PASSWORD: 1,
      REMEMBER_PASSWORD: 2,
      CONFIG: 3,
      ALLOW_EXISTING: 4,
    };

    setup(function() {
      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      createDialog(null);
    });

    teardown(function() {
      dialog.remove();
    });

    function createDialog(presetAccount) {
      if (dialog) {
        dialog.remove();
      }

      dialog = document.createElement('kerberos-add-account-dialog');
      dialog.presetAccount = presetAccount;
      document.body.appendChild(dialog);

      addDialog = dialog.$.addDialog;
      assertTrue(!!addDialog);

      username = dialog.$.username;
      assertTrue(!!username);

      password = dialog.$.password;
      assertTrue(!!password);

      rememberPassword = dialog.$.rememberPassword;
      assertTrue(!!rememberPassword);

      advancedConfigButton = dialog.$.advancedConfigButton;
      assertTrue(!!advancedConfigButton);

      addButton = addDialog.querySelector('.action-button');
      assertTrue(!!addButton);

      generalError = dialog.$['general-error-message'];
      assertTrue(!!generalError);
    }

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

    // Opens the Advanced Config dialog, sets |config| as Kerberos configuration
    // and clicks 'Save'.
    function setConfig(config) {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      const configElement = advancedConfigDialog.querySelector('#config');
      assertFalse(configElement.disabled);
      configElement.value = config;
      advancedConfigDialog.querySelector('.action-button').click();
      Polymer.dom.flush();
    }

    // Opens the Advanced Config dialog, asserts that |config| is set as
    // Kerberos configuration and clicks 'Cancel'.
    function assertConfig(config) {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertEquals(config, advancedConfigDialog.querySelector('#config').value);
      advancedConfigDialog.querySelector('.cancel-button').click();
      Polymer.dom.flush();
    }

    // Verifies expected states if no account is preset.
    test('StatesWithoutPresetAccount', function() {
      assertFalse(username.disabled);
      assertEquals('', username.value);
      assertEquals('', password.value);
      assertConfig(loadTimeData.getString('defaultKerberosConfig'));
      assertFalse(rememberPassword.checked);
    });

    // Verifies expected states if an account is preset.
    test('StatesWithPresetAccount', function() {
      createDialog(testAccounts[0]);
      assertTrue(username.readonly);
      assertEquals(testAccounts[0].principalName, username.value);
      assertConfig(testAccounts[0].config);
      // Password and remember password are tested below since the contents
      // depends on the passwordWasRemembered property of the account.
    });

    // The password input field is empty and 'Remember password' is not preset
    // if |passwordWasRemembered| is false.
    test('PasswordNotPresetIfPasswordWasNotRemembered', function() {
      assertFalse(testAccounts[0].passwordWasRemembered);
      createDialog(testAccounts[0]);
      assertEquals('', password.value);
      assertFalse(rememberPassword.checked);
    });

    // The password input field is not empty and 'Remember password' is preset
    // if |passwordWasRemembered| is true.
    test('PasswordPresetIfPasswordWasRemembered', function() {
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);
      assertNotEquals('', password.value);
      assertTrue(rememberPassword.checked);
    });

    test('RememberPasswordEnabled', function() {
      assertTrue(loadTimeData.getBoolean('kerberosRememberPasswordEnabled'));
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);

      assertTrue(!dialog.$$('#rememberPasswordPolicyIndicator'));
      assertFalse(rememberPassword.disabled);
      assertTrue(rememberPassword.checked);
      assertNotEquals('', password.value);
    });

    test('RememberPasswordDisabled', function() {
      loadTimeData.overrideValues({kerberosRememberPasswordEnabled: false});
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);
      Polymer.dom.flush();

      assertTrue(!!dialog.$$('#rememberPasswordPolicyIndicator'));
      assertTrue(rememberPassword.disabled);
      assertFalse(rememberPassword.checked);
      assertEquals('', password.value);

      // Reset for further tests.
      loadTimeData.overrideValues({kerberosRememberPasswordEnabled: true});
    });

    // By clicking the "Add account", all field values are passed to the
    // 'addAccount' browser proxy method.
    test('AddButtonPassesFieldValues', function() {
      const EXPECTED_USER = 'testuser';
      const EXPECTED_PASS = 'testpass';
      const EXPECTED_REMEMBER_PASS = true;
      const EXPECTED_CONFIG = 'testconf';

      username.value = EXPECTED_USER;
      password.value = EXPECTED_PASS;
      setConfig(EXPECTED_CONFIG);
      rememberPassword.checked = EXPECTED_REMEMBER_PASS;

      assertFalse(addButton.disabled);
      addButton.click();
      return browserProxy.whenCalled('addAccount').then(function(args) {
        assertEquals(EXPECTED_USER, args[AddParams.PRINCIPAL_NAME]);
        assertEquals(EXPECTED_PASS, args[AddParams.PASSWORD]);
        assertEquals(EXPECTED_REMEMBER_PASS, args[AddParams.REMEMBER_PASSWORD]);
        assertEquals(EXPECTED_CONFIG, args[AddParams.CONFIG]);

        // Should be false if a new account is added. See also
        // AllowExistingIsTrueForPresetAccounts test.
        assertFalse(args[AddParams.ALLOW_EXISTING]);
      });
    });

    // If an account is preset, overwriting that account should be allowed.
    test('AllowExistingIsTrueForPresetAccounts', function() {
      // Populate dialog with preset account.
      createDialog(testAccounts[1]);
      addButton.click();
      return browserProxy.whenCalled('addAccount').then(function(args) {
        assertTrue(args[AddParams.ALLOW_EXISTING]);
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

    // If the account has passwordWasRemembered == true and the user just clicks
    // the 'Add' button, an empty password is submitted.
    test('SubmitsEmptyPasswordIfRememberedPasswordIsUsed', function() {
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);
      addButton.click();
      return browserProxy.whenCalled('addAccount').then(function(args) {
        assertEquals('', args[AddParams.PASSWORD]);
        assertTrue(args[AddParams.REMEMBER_PASSWORD]);
      });
    });

    // If the account has passwordWasRemembered == true and the user changes the
    // password before clicking 'Add' button, the changed password is submitted.
    test('SubmitsChangedPasswordIfRememberedPasswordIsChanged', function() {
      assertTrue(testAccounts[1].passwordWasRemembered);
      createDialog(testAccounts[1]);
      password.inputElement.value = 'some edit';
      password.dispatchEvent(new CustomEvent('input'));
      addButton.click();
      return browserProxy.whenCalled('addAccount').then(function(args) {
        assertNotEquals('', args[AddParams.PASSWORD]);
        assertTrue(args[AddParams.REMEMBER_PASSWORD]);
      });
    });

    test('AdvancedConfigOpenClose', function() {
      assertTrue(!dialog.$$('#advancedConfigDialog'));
      assertFalse(addDialog.hidden);
      advancedConfigButton.click();
      Polymer.dom.flush();

      let advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);
      assertTrue(advancedConfigDialog.open);
      assertTrue(addDialog.hidden);
      advancedConfigDialog.querySelector('.action-button').click();

      Polymer.dom.flush();
      assertTrue(!dialog.$$('#advancedConfigDialog'));
      assertFalse(addDialog.hidden);
      assertTrue(addDialog.open);
    });

    test('AdvancedConfigurationSaveKeepsConfig', function() {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);

      // Change config and save.
      const modifiedConfig = 'modified';
      advancedConfigDialog.querySelector('#config').value = modifiedConfig;
      advancedConfigDialog.querySelector('.action-button').click();

      // Changed value should stick.
      Polymer.dom.flush();
      assertConfig(modifiedConfig);
    });

    test('AdvancedConfigurationCancelResetsConfig', function() {
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);

      // Change config and cancel.
      const prevConfig = advancedConfigDialog.querySelector('#config').value;
      advancedConfigDialog.querySelector('#config').value = 'modified';
      advancedConfigDialog.querySelector('.cancel-button').click();
      Polymer.dom.flush();

      // Changed value should NOT stick.
      assertConfig(prevConfig);
    });

    test('AdvancedConfigurationDisabledByPolicy', function() {
      assertTrue(testAccounts[2].isManaged);
      createDialog(testAccounts[2]);
      advancedConfigButton.click();
      Polymer.dom.flush();
      const advancedConfigDialog = dialog.$$('#advancedConfigDialog');
      assertTrue(!!advancedConfigDialog);
      assertTrue(!!advancedConfigDialog.querySelector(
          '#advancedConfigPolicyIndicator'));
      assertTrue(advancedConfigDialog.querySelector('#config').disabled);
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

    // addAccount: KerberosErrorType.kDuplicatePrincipalName spawns a username
    // error.
    test('AddAccountError_DuplicatePrincipalName', function() {
      checkAddAccountError(
          settings.KerberosErrorType.kDuplicatePrincipalName, username.$.error);
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