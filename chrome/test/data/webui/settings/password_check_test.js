// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Check Password tests. */

// clang-format off
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// #import {OpenWindowProxyImpl, PasswordManagerProxy, PasswordManagerImpl, routes, Router} from 'chrome://settings/settings.js';
// #import 'chrome://settings/lazy_load.js';
// #import {makeCompromisedCredential,  makePasswordCheckStatus} from 'chrome://test/settings/passwords_and_autofill_fake_data.m.js';
// #import {getSyncAllPrefs,simulateSyncStatus} from 'chrome://test/settings/sync_test_util.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {TestOpenWindowProxy} from 'chrome://test/settings/test_open_window_proxy.m.js';
// #import {TestPasswordManagerProxy} from 'chrome://test/settings/test_password_manager_proxy.m.js';
// clang-format on

cr.define('settings_passwords_check', function() {
  const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

  function createCheckPasswordSection() {
    // Create a passwords-section to use for testing.
    const passwordsSection = document.createElement('settings-password-check');
    document.body.appendChild(passwordsSection);
    Polymer.dom.flush();
    return passwordsSection;
  }

  function createEditDialog(leakedCredential) {
    const editDialog =
        document.createElement('settings-password-check-edit-dialog');
    editDialog.item = leakedCredential;
    document.body.appendChild(editDialog);
    Polymer.dom.flush();
    return editDialog;
  }

  /**
   * Helper method used to create a compromised list item.
   * @param {!chrome.passwordsPrivate.CompromisedCredential} entry
   * @return {!PasswordCheckListItemElement}
   */
  function createLeakedPasswordItem(entry) {
    const leakedPasswordItem =
        document.createElement('password-check-list-item');
    leakedPasswordItem.item = entry;
    document.body.appendChild(leakedPasswordItem);
    Polymer.dom.flush();
    return leakedPasswordItem;
  }

  function isElementVisible(element) {
    return !!element && !element.hidden && element.style.display != 'none' &&
        element.offsetParent !== null;  // Considers parents hiding |element|.
  }


  /**
   * Helper method used to create a remove password confirmation dialog.
   * @param {!chrome.passwordsPrivate.CompromisedCredential} entry
   */
  function createRemovePasswordDialog(entry) {
    const element =
        document.createElement('settings-password-remove-confirmation-dialog');
    element.item = entry;
    document.body.appendChild(element);
    Polymer.dom.flush();
    return element;
  }

  /**
   * Helper method used to randomize array.
   * @param {!Array<!chrome.passwordsPrivate.CompromisedCredential>} array
   * @return {!Array<!chrome.passwordsPrivate.CompromisedCredential>}
   */
  function shuffleArray(array) {
    const copy = array.slice();
    for (let i = copy.length - 1; i > 0; i--) {
      const j = Math.floor(Math.random() * (i + 1));
      const temp = copy[i];
      copy[i] = copy[j];
      copy[j] = temp;
    }
    return copy;
  }

  /**
   * Helper method that validates a that elements in the compromised credentials
   * list match the expected data.
   * @param {!Element} checkPasswordSection The section element that will be
   *     checked.
   * @param {!Array<!chrome.passwordsPrivate.CompromisedCredential>}
   * passwordList The expected data.
   * @private
   */
  function validateLeakedPasswordsList(
      checkPasswordSection, compromisedCredentials) {
    const listElements = checkPasswordSection.$.leakedPasswordList;
    assertEquals(listElements.items.length, compromisedCredentials.length);
    const nodes = checkPasswordSection.shadowRoot.querySelectorAll(
        'password-check-list-item');
    for (let index = 0; index < compromisedCredentials.length; ++index) {
      const node = nodes[index];
      assertTrue(!!node);
      assertEquals(
          node.$.elapsedTime.textContent.trim(),
          compromisedCredentials[index].elapsedTimeSinceCompromise);
      assertEquals(
          node.$.leakUsername.textContent.trim(),
          compromisedCredentials[index].username);
      assertEquals(
          node.$.leakOrigin.textContent.trim(),
          compromisedCredentials[index].formattedOrigin);
    }
  }

  suite('PasswordsCheckSection', function() {
    /** @type {TestPasswordManagerProxy} */
    let passwordManager = null;

    suiteSetup(function() {
      loadTimeData.overrideValues({enablePasswordCheck: true});
    });

    setup(function() {
      PolymerTest.clearBody();
      // Override the PasswordManagerImpl for testing.
      passwordManager = new TestPasswordManagerProxy();
      PasswordManagerImpl.instance_ = passwordManager;
    });

    // Test verifies that clicking 'Check again' make proper function call to
    // password manager
    test('checkAgainButtonWhenIdleAfterFirstRun', async function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.IDLE,
          /*checked=*/ undefined,
          /*remaining=*/ undefined,
          /*lastCheck=*/ 'Just now');
      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
      expectEquals(
          section.i18n('checkPasswordsAgain'),
          section.$.controlPasswordCheckButton.innerText);
      section.$.controlPasswordCheckButton.click();
      await passwordManager.whenCalled('startBulkPasswordCheck');
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY,
          interaction);
    });

    // Test verifies that clicking 'Start Check' make proper function call to
    // password manager
    test('startCheckButtonWhenIdle', async function() {
      assertEquals(
          PasswordCheckState.IDLE, passwordManager.data.checkStatus.state);
      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
      expectEquals(
          section.i18n('checkPasswords'),
          section.$.controlPasswordCheckButton.innerText);
      section.$.controlPasswordCheckButton.click();
      await passwordManager.whenCalled('startBulkPasswordCheck');
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY,
          interaction);
    });

    // Test verifies that clicking 'Check again' make proper function call to
    // password manager
    test('stopButtonWhenRunning', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 0,
              /*remaining=*/ 2);
      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
      expectEquals(
          section.i18n('checkPasswordsStop'),
          section.$.controlPasswordCheckButton.innerText);
      section.$.controlPasswordCheckButton.click();
      await passwordManager.whenCalled('stopBulkPasswordCheck');
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.STOP_CHECK,
          interaction);
    });

    // Test verifies that sync users see only the link to account checkup and no
    // button to start the local leak check once they run out of quota.
    test('onlyCheckupLinkAfterHittingQuotaWhenSyncing', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.QUOTA_LIMIT);

      const section = createCheckPasswordSection();
      cr.webUIListenerCallback(
          'sync-prefs-changed', sync_test_util.getSyncAllPrefs());
      sync_test_util.simulateSyncStatus({signedIn: true});

      await passwordManager.whenCalled('getPasswordCheckStatus');
      expectTrue(isElementVisible(section.$.linkToGoogleAccount));
      expectFalse(isElementVisible(section.$.controlPasswordCheckButton));
    });

    // Test verifies that non-sync users see neither the link to the account
    // checkup nor a retry button once they run out of quota.
    test('noCheckupLinkAfterHittingQuotaWhenSignedOut', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.QUOTA_LIMIT);

      const section = createCheckPasswordSection();
      cr.webUIListenerCallback(
          'sync-prefs-changed', sync_test_util.getSyncAllPrefs());
      sync_test_util.simulateSyncStatus({signedIn: false});

      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      expectFalse(isElementVisible(section.$.linkToGoogleAccount));
      expectFalse(isElementVisible(section.$.controlPasswordCheckButton));
    });

    // Test verifies that custom passphrase users see neither the link to the
    // account checkup nor a retry button once they run out of quota.
    test('noCheckupLinkAfterHittingQuotaForEncryption', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.QUOTA_LIMIT);

      const section = createCheckPasswordSection();
      const syncPrefs = sync_test_util.getSyncAllPrefs();
      syncPrefs.encryptAllData = true;
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      sync_test_util.simulateSyncStatus({signedIn: true});

      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      expectFalse(isElementVisible(section.$.linkToGoogleAccount));
      assertFalse(isElementVisible(section.$.controlPasswordCheckButton));
    });

    // Test verifies that 'Try again' visible and working when users encounter a
    // generic error.
    test('showRetryAfterGenericError', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.OTHER_ERROR);
      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
      expectEquals(
          section.i18n('checkPasswordsAgainAfterError'),
          section.$.controlPasswordCheckButton.innerText);
      section.$.controlPasswordCheckButton.click();
      await passwordManager.whenCalled('startBulkPasswordCheck');
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY,
          interaction);
    });

    // Test verifies that 'Try again' is hidden when users encounter a
    // not-signed-in error.
    test('hideRetryAfterSignOutErrorUntilSignedInAgain', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.SIGNED_OUT);
      const section = createCheckPasswordSection();
      cr.webUIListenerCallback('stored-accounts-updated', []);
      if (cr.isChromeOS) {
        sync_test_util.simulateSyncStatus({signedIn: false});
      }
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      expectFalse(isElementVisible(section.$.controlPasswordCheckButton));
      cr.webUIListenerCallback(
          'stored-accounts-updated', [{email: 'foo@bar.com'}]);
      if (cr.isChromeOS) {
        sync_test_util.simulateSyncStatus({signedIn: true, hasError: false});
      }
      assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
      expectEquals(
          section.i18n('checkPasswordsAgainAfterError'),
          section.$.controlPasswordCheckButton.innerText);
      section.$.controlPasswordCheckButton.click();
      await passwordManager.whenCalled('startBulkPasswordCheck');
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY,
          interaction);
    });

    // Test verifies that 'Try again' is hidden when users encounter a
    // no-saved-passwords error.
    test('hideRetryAfterNoPasswordsError', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.NO_PASSWORDS);
      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      expectFalse(isElementVisible(section.$.controlPasswordCheckButton));
    });

    // Test verifies that 'Try again' visible and working when users encounter a
    // connection error.
    test('showRetryAfterNoConnectionError', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.OFFLINE);
      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
      expectEquals(
          section.i18n('checkPasswordsAgainAfterError'),
          section.$.controlPasswordCheckButton.innerText);
      section.$.controlPasswordCheckButton.click();
      await passwordManager.whenCalled('startBulkPasswordCheck');
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY,
          interaction);
    });

    // Test verifies that if no compromised credentials found than list is not
    // shown
    test('noCompromisedCredentials', async function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.IDLE,
          /*checked=*/ 4,
          /*remaining=*/ 0,
          /*lastCheck=*/ 'Just now');
      data.leakedCredentials = [];

      const section = createCheckPasswordSection();
      assertFalse(isElementVisible(section.$.noCompromisedCredentials));
      cr.webUIListenerCallback(
          'sync-prefs-changed', sync_test_util.getSyncAllPrefs());
      sync_test_util.simulateSyncStatus({signedIn: true});

      // Initialize with dummy data breach detection settings
      section.prefs = {
        profile: {password_manager_leak_detection: {value: true}}
      };

      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      assertFalse(isElementVisible(section.$.passwordCheckBody));
      assertTrue(isElementVisible(section.$.noCompromisedCredentials));
    });

    // Test verifies that compromised credentials are displayed in a proper way
    test('someCompromisedCredentials', async function() {
      const leakedPasswords = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'PHISHED', 1, 1),
        autofill_test_util.makeCompromisedCredential(
            'two.com', 'test3', 'LEAKED', 2, 2),
      ];
      passwordManager.data.leakedCredentials = leakedPasswords;
      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getCompromisedCredentials');
      Polymer.dom.flush();
      assertFalse(checkPasswordSection.$.passwordCheckBody.hidden);
      assertTrue(checkPasswordSection.$.noCompromisedCredentials.hidden);
      validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
    });

    // Test verifies that credentials from mobile app shown correctly
    test('someCompromisedCredentials', function() {
      const password = autofill_test_util.makeCompromisedCredential(
          'one.com', 'test4', 'LEAKED');
      password.changePasswordUrl = null;

      const checkPasswordSection = createLeakedPasswordItem(password);
      assertEquals(checkPasswordSection.$$('changePasswordUrl'), null);
      assertTrue(!!checkPasswordSection.$$('#changePasswordInApp'));
    });

    // Verify that a click on "Change password" opens the expected URL and
    // records a corresponding user action.
    test('changePasswordOpensUrlAndRecordsAction', async function() {
      const testOpenWindowProxy = new TestOpenWindowProxy();
      settings.OpenWindowProxyImpl.instance_ = testOpenWindowProxy;

      const password = autofill_test_util.makeCompromisedCredential(
          'one.com', 'test4', 'LEAKED');
      const passwordCheckListItem = createLeakedPasswordItem(password);
      passwordCheckListItem.$$('#changePasswordButton').click();

      const url = await testOpenWindowProxy.whenCalled('openURL');
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      assertEquals('http://one.com/', url);
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.CHANGE_PASSWORD,
          interaction);
    });

    // Verify that the More Actions menu opens when the button is clicked.
    test('moreActionsMenu', async function() {
      const leakedPasswords = [
        autofill_test_util.makeCompromisedCredential(
            'google.com', 'jdoerrie', 'LEAKED'),
      ];
      passwordManager.data.leakedCredentials = leakedPasswords;
      const checkPasswordSection = createCheckPasswordSection();

      await passwordManager.whenCalled('getCompromisedCredentials');
      Polymer.dom.flush();
      assertFalse(checkPasswordSection.$.passwordCheckBody.hidden);
      const listElement = checkPasswordSection.$$('password-check-list-item');
      const menu = checkPasswordSection.$.moreActionsMenu;

      assertFalse(menu.open);
      listElement.$.more.click();
      assertTrue(menu.open);
    });

    // Test verifies that clicking remove button is calling proper
    // proxy function.
    test('removePasswordConfirmationDialog', async function() {
      const entry = autofill_test_util.makeCompromisedCredential(
          'one.com', 'test4', 'LEAKED', 0);
      const removeDialog = createRemovePasswordDialog(entry);
      removeDialog.$.remove.click();
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      const {id, username, formattedOrigin} =
          await passwordManager.whenCalled('removeCompromisedCredential');

      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.REMOVE_PASSWORD,
          interaction);
      assertEquals(0, id);
      assertEquals('test4', username);
      assertEquals('one.com', formattedOrigin);
    });

    // A changing status is immediately reflected in title, icon and banner.
    test('updatesNumberOfCheckedPasswordsWhileRunning', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 0,
              /*remaining=*/ 2);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      assertTrue(isElementVisible(section.$.title));
      expectEquals(
          section.i18n('checkPasswordsProgress', 1, 2),
          section.$.title.innerText);

      // Change status from running to IDLE.
      assertTrue(!!passwordManager.lastCallback.addPasswordCheckStatusListener);
      passwordManager.lastCallback.addPasswordCheckStatusListener(
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 1,
              /*remaining=*/ 1));

      Polymer.dom.flush();
      assertTrue(isElementVisible(section.$.title));
      expectEquals(
          section.i18n('checkPasswordsProgress', 2, 2),
          section.$.title.innerText);
    });

    // Tests that the status is queried right when the page loads.
    test('queriesCheckedStatusImmediately', async function() {
      const data = passwordManager.data;
      assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
      assertEquals(0, data.leakedCredentials.length);

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      expectEquals(PasswordCheckState.IDLE, checkPasswordSection.status_.state);
    });

    // Tests that the spinner is replaced with a checkmark on successful runs.
    test('showsCheckmarkIconWhenFinishedWithoutLeaks', async function() {
      const data = passwordManager.data;
      assertEquals(0, data.leakedCredentials.length);
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.IDLE,
          /*checked=*/ undefined,
          /*remaining=*/ undefined,
          /*lastCheck=*/ 'Just now');

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const icon = checkPasswordSection.$$('iron-icon');
      const spinner = checkPasswordSection.$$('paper-spinner-lite');
      expectFalse(isElementVisible(spinner));
      assertTrue(isElementVisible(icon));
      expectFalse(icon.classList.contains('has-leaks'));
      expectTrue(icon.classList.contains('no-leaks'));
    });

    // Tests that there is neither spinner nor icon if the check hasn't run yet.
    test('iconWhenFirstRunIsPending', async function() {
      const data = passwordManager.data;
      assertEquals(0, data.leakedCredentials.length);
      data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(PasswordCheckState.IDLE);

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const icon = checkPasswordSection.$$('iron-icon');
      const spinner = checkPasswordSection.$$('paper-spinner-lite');
      expectFalse(isElementVisible(spinner));
      expectFalse(isElementVisible(icon));
    });

    // Tests that the spinner is replaced with a triangle if leaks were found.
    test('showsTriangleIconWhenFinishedWithLeaks', async function() {
      const data = passwordManager.data;
      assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
      data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'LEAKED'),
      ];

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const icon = checkPasswordSection.$$('iron-icon');
      const spinner = checkPasswordSection.$$('paper-spinner-lite');
      expectFalse(isElementVisible(spinner));
      assertTrue(isElementVisible(icon));
      expectTrue(icon.classList.contains('has-leaks'));
      expectFalse(icon.classList.contains('no-leaks'));
    });

    // Tests that the spinner is replaced with a warning on errors.
    test('showsInfoIconWhenFinishedWithErrors', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.OFFLINE,
              /*checked=*/ undefined,
              /*remaining=*/ undefined);

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const icon = checkPasswordSection.$$('iron-icon');
      const spinner = checkPasswordSection.$$('paper-spinner-lite');
      expectFalse(isElementVisible(spinner));
      assertTrue(isElementVisible(icon));
      expectFalse(icon.classList.contains('has-leaks'));
      expectFalse(icon.classList.contains('no-leaks'));
    });

    // Tests that the spinner replaces any icon while the check is running.
    test('showsSpinnerWhileRunning', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 1,
              /*remaining=*/ 3);

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const icon = checkPasswordSection.$$('iron-icon');
      const spinner = checkPasswordSection.$$('paper-spinner-lite');
      expectTrue(isElementVisible(spinner));
      expectFalse(isElementVisible(icon));
    });

    // While running, the check should show the processed and total passwords.
    test('showOnlyProgressWhileRunningWithoutLeaks', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 0,
              /*remaining=*/ 4);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      assertTrue(isElementVisible(title));
      expectEquals(
          section.i18n('checkPasswordsProgress', 1, 4), title.innerText);
      expectFalse(isElementVisible(section.$.subtitle));
    });

    // Verifies that in case the backend could not obtain the number of checked
    // and remaining credentials the UI does not surface 0s to the user.
    test('runningProgressHandlesZeroCaseFromBackend', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 0,
              /*remaining=*/ 0);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      assertTrue(isElementVisible(title));
      expectEquals(
          section.i18n('checkPasswordsProgress', 1, 1), title.innerText);
      expectFalse(isElementVisible(section.$.subtitle));
    });

    // While running, show progress and already found leak count.
    test('showProgressAndLeaksWhileRunning', async function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.RUNNING,
          /*checked=*/ 1,
          /*remaining=*/ 4);
      data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'LEAKED'),
      ];

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      const subtitle = section.$.subtitle;
      assertTrue(isElementVisible(title));
      assertTrue(isElementVisible(subtitle));
      expectEquals(
          section.i18n('checkPasswordsProgress', 2, 5), title.innerText);
    });

    // When canceled, show string explaining that and already found leak
    // count.
    test('showProgressAndLeaksAfterCanceled', async function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.CANCELED,
          /*checked=*/ 2,
          /*remaining=*/ 3);
      data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'LEAKED'),
      ];

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      const subtitle = section.$.subtitle;
      assertTrue(isElementVisible(title));
      assertTrue(isElementVisible(subtitle));
      expectEquals(section.i18n('checkPasswordsCanceled'), title.innerText);
    });

    // Before the first run, show only a description of what the check does.
    test('showOnlyDescriptionIfNotRun', async function() {
      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      const subtitle = section.$.subtitle;
      assertTrue(isElementVisible(title));
      assertFalse(isElementVisible(subtitle));
      expectEquals(section.i18n('checkPasswordsDescription'), title.innerText);
    });

    // After running, show confirmation, timestamp and number of leaks.
    test('showLeakCountAndTimeStampWhenIdle', async function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.IDLE,
          /*checked=*/ 4,
          /*remaining=*/ 0,
          /*lastCheck=*/ 'Just now');
      data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'LEAKED'),
      ];

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      const subtitle = section.$.subtitle;
      assertTrue(isElementVisible(title));
      assertTrue(isElementVisible(subtitle));
      expectEquals(
          section.i18n('checkedPasswords') + ' • Just now', title.innerText);
    });

    // When offline, only show an error.
    test('showOnlyErrorWhenOffline', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.OFFLINE);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      assertTrue(isElementVisible(title));
      expectEquals(section.i18n('checkPasswordsErrorOffline'), title.innerText);
      expectFalse(isElementVisible(section.$.subtitle));
    });

    // When signed out, only show an error.
    test('showOnlyErrorWhenSignedOut', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.SIGNED_OUT);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      assertTrue(isElementVisible(title));
      expectEquals(
          section.i18n('checkPasswordsErrorSignedOut'), title.innerText);
      expectFalse(isElementVisible(section.$.subtitle));
    });

    // When no passwords are saved, only show an error.
    test('showOnlyErrorWithoutPasswords', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.NO_PASSWORDS);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      assertTrue(isElementVisible(title));
      expectEquals(
          section.i18n('checkPasswordsErrorNoPasswords'), title.innerText);
      expectFalse(isElementVisible(section.$.subtitle));
    });

    // When users run out of quota, only show an error.
    test('showOnlyErrorWhenQuotaIsHit', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.QUOTA_LIMIT);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      assertTrue(isElementVisible(title));
      expectEquals(section.i18n('checkPasswordsErrorQuota'), title.innerText);
      expectFalse(isElementVisible(section.$.subtitle));
    });

    // When a general error occurs, only show the message.
    test('showOnlyGenericError', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.OTHER_ERROR);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      const title = section.$.title;
      assertTrue(isElementVisible(title));
      expectEquals(section.i18n('checkPasswordsErrorGeneric'), title.innerText);
      expectFalse(isElementVisible(section.$.subtitle));
    });

    // Transform check-button to stop-button if a check is running.
    test('buttonChangesTextAccordingToStatus', async function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(PasswordCheckState.IDLE);

      const section = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
      expectEquals(
          section.i18n('checkPasswords'),
          section.$.controlPasswordCheckButton.innerText);

      // Change status from running to IDLE.
      assertTrue(!!passwordManager.lastCallback.addPasswordCheckStatusListener);
      passwordManager.lastCallback.addPasswordCheckStatusListener(
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 0,
              /*remaining=*/ 2));
      assertTrue(isElementVisible(section.$.controlPasswordCheckButton));
      expectEquals(
          section.i18n('checkPasswordsStop'),
          section.$.controlPasswordCheckButton.innerText);
    });

    // Test that the banner is in a state that shows the positive confirmation
    // after a leak check finished.
    test('showsPositiveBannerWhenIdle', async function() {
      const data = passwordManager.data;
      assertEquals(0, data.leakedCredentials.length);
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.IDLE,
          /*checked=*/ undefined,
          /*remaining=*/ undefined,
          /*lastCheck=*/ 'Just now');

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      assertTrue(isElementVisible(checkPasswordSection.$$('#bannerImage')));
      expectEquals(
          'chrome://settings/images/password_check_positive.svg',
          checkPasswordSection.$$('#bannerImage').src);
    });

    // Test that the banner indicates a neutral state if no check was run yet.
    test('showsNeutralBannerBeforeFirstRun', async function() {
      const data = passwordManager.data;
      assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
      assertEquals(0, data.leakedCredentials.length);

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      assertTrue(isElementVisible(checkPasswordSection.$$('#bannerImage')));
      expectEquals(
          'chrome://settings/images/password_check_neutral.svg',
          checkPasswordSection.$$('#bannerImage').src);
    });

    // Test that the banner is in a state that shows that the leak check is
    // in progress but hasn't found anything yet.
    test('showsNeutralBannerWhenRunning', async function() {
      const data = passwordManager.data;
      assertEquals(0, data.leakedCredentials.length);
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.RUNNING, /*checked=*/ 1,
          /*remaining=*/ 5);

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      assertTrue(isElementVisible(checkPasswordSection.$$('#bannerImage')));
      expectEquals(
          'chrome://settings/images/password_check_neutral.svg',
          checkPasswordSection.$$('#bannerImage').src);
    });

    // Test that the banner is in a state that shows that the leak check is
    // in progress but hasn't found anything yet.
    test('showsNeutralBannerWhenCanceled', async function() {
      const data = passwordManager.data;
      assertEquals(0, data.leakedCredentials.length);
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.CANCELED);

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      assertTrue(isElementVisible(checkPasswordSection.$$('#bannerImage')));
      expectEquals(
          'chrome://settings/images/password_check_neutral.svg',
          checkPasswordSection.$$('#bannerImage').src);
    });

    // Test that the banner isn't visible as soon as the first leak is detected.
    test('leaksHideBannerWhenRunning', async function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.RUNNING, /*checked=*/ 1,
          /*remaining=*/ 5);
      data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'LEAKED'),
      ];

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      expectFalse(isElementVisible(checkPasswordSection.$$('#bannerImage')));
    });

    // Test that the banner isn't visible if a leak is detected after a check.
    test('leaksHideBannerWhenIdle', async function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.IDLE);
      data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'LEAKED'),
      ];

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      expectFalse(isElementVisible(checkPasswordSection.$$('#bannerImage')));
    });

    // Test that the banner isn't visible if a leak is detected after canceling.
    test('leaksHideBannerWhenCanceled', async function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.CANCELED);
      data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'LEAKED'),
      ];

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getPasswordCheckStatus');
      Polymer.dom.flush();
      expectFalse(isElementVisible(checkPasswordSection.$$('#bannerImage')));
    });

    // Test verifies that new credentials are added to the bottom
    test('appendCompromisedCredentials', function() {
      const leakedPasswords = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'LEAKED', 1, 0),
        autofill_test_util.makeCompromisedCredential(
            'two.com', 'test3', 'LEAKED', 2, 0),
      ];
      const checkPasswordSection = createCheckPasswordSection();
      checkPasswordSection.updateList_(leakedPasswords);
      Polymer.dom.flush();

      validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);

      leakedPasswords.push(autofill_test_util.makeCompromisedCredential(
          'three.com', 'test2', 'PHISHED', 3, 3));
      leakedPasswords.push(autofill_test_util.makeCompromisedCredential(
          'four.com', 'test1', 'LEAKED', 4, 5));
      leakedPasswords.push(autofill_test_util.makeCompromisedCredential(
          'five.com', 'test0', 'LEAKED', 5, 4));
      checkPasswordSection.updateList_(shuffleArray(leakedPasswords));
      Polymer.dom.flush();
      validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
    });

    // Test verifies that deleting and adding works as it should
    test('deleteComrpomisedCredemtials', function() {
      const leakedPasswords = [
        autofill_test_util.makeCompromisedCredential(
            'one.com', 'test4', 'PHISHED', 0, 0),
        autofill_test_util.makeCompromisedCredential(
            'two.com', 'test3', 'LEAKED', 1, 2),
        autofill_test_util.makeCompromisedCredential(
            'three.com', 'test2', 'LEAKED', 2, 2),
        autofill_test_util.makeCompromisedCredential(
            'four.com', 'test2', 'LEAKED', 3, 2),
      ];
      const checkPasswordSection = createCheckPasswordSection();
      checkPasswordSection.updateList_(leakedPasswords);
      Polymer.dom.flush();
      validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);

      // remove 2nd and 3rd elements
      leakedPasswords.splice(1, 2);
      leakedPasswords.push(autofill_test_util.makeCompromisedCredential(
          'five.com', 'test2', 'LEAKED', 4, 3));

      checkPasswordSection.updateList_(shuffleArray(leakedPasswords));
      Polymer.dom.flush();
      validateLeakedPasswordsList(checkPasswordSection, leakedPasswords);
    });

    // Verify that the edit dialog is not shown if a plaintext password could
    // not be obtained.
    test('editDialogWithoutPlaintextPassword', async function() {
      passwordManager.data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'google.com', 'jdoerrie', 'LEAKED'),
      ];

      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getCompromisedCredentials');
      Polymer.dom.flush();
      const listElements = checkPasswordSection.$.leakedPasswordList;
      const node = listElements.children[1];

      // Open the more actions menu and click 'Edit Password'.
      node.$.more.click();
      checkPasswordSection.$.menuEditPassword.click();
      // Since we did not specify a plaintext password above, this request
      // should fail.
      await passwordManager.whenCalled('getPlaintextCompromisedPassword');
      // Verify that the edit dialog has not become visible.
      Polymer.dom.flush();
      expectFalse(isElementVisible(
          checkPasswordSection.$$('settings-password-check-edit-dialog')));

      // Verify that the more actions menu is closed.
      expectFalse(checkPasswordSection.$.moreActionsMenu.open);
    });

    // Verify edit a password on the edit dialog.
    test('editDialogWithPlaintextPassword', async function() {
      passwordManager.data.leakedCredentials = [
        autofill_test_util.makeCompromisedCredential(
            'google.com', 'jdoerrie', 'LEAKED'),
      ];

      passwordManager.setPlaintextPassword('password');
      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getCompromisedCredentials');
      Polymer.dom.flush();
      const listElements = checkPasswordSection.$.leakedPasswordList;
      const node = listElements.children[1];

      // Open the more actions menu and click 'Edit Password'.
      node.$.more.click();
      checkPasswordSection.$.menuEditPassword.click();
      const {credential, reason} =
          await passwordManager.whenCalled('getPlaintextCompromisedPassword');
      expectEquals(passwordManager.data.leakedCredentials[0], credential);
      expectEquals(chrome.passwordsPrivate.PlaintextReason.EDIT, reason);

      // Verify that the edit dialog has become visible.
      Polymer.dom.flush();
      expectTrue(isElementVisible(
          checkPasswordSection.$$('settings-password-check-edit-dialog')));

      // Verify that the more actions menu is closed.
      expectFalse(checkPasswordSection.$.moreActionsMenu.open);
    });

    test('editDialogChangePassword', async function() {
      const leakedPassword = autofill_test_util.makeCompromisedCredential(
          'google.com', 'jdoerrie', 'LEAKED');
      leakedPassword.password = 'mybirthday';
      const editDialog = createEditDialog(leakedPassword);

      assertEquals(leakedPassword.password, editDialog.$.passwordInput.value);
      editDialog.$.passwordInput.value = 'yadhtribym';
      editDialog.$.save.click();

      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      const {newPassword} =
          await passwordManager.whenCalled('changeCompromisedCredential');
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.EDIT_PASSWORD,
          interaction);
      assertEquals('yadhtribym', newPassword);
    });

    test('editDialogCancel', function() {
      const leakedPassword = autofill_test_util.makeCompromisedCredential(
          'google.com', 'jdoerrie', 'LEAKED');
      leakedPassword.password = 'mybirthday';
      const editDialog = createEditDialog(leakedPassword);

      assertEquals(leakedPassword.password, editDialog.$.passwordInput.value);
      editDialog.$.passwordInput.value = 'yadhtribym';
      editDialog.$.cancel.click();

      assertEquals(
          0, passwordManager.getCallCount('changeCompromisedCredential'));
    });

    test('startEqualsTrueSearchParameterStartsCheck', async function() {
      settings.Router.getInstance().navigateTo(
          settings.routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
      createCheckPasswordSection();
      await passwordManager.whenCalled('startBulkPasswordCheck');
      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');
      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction
              .START_CHECK_AUTOMATICALLY,
          interaction);
      settings.Router.getInstance().resetRouteForTesting();
    });

    // Verify clicking show password in menu reveal password.
    test('showHidePasswordMenuItemSuccess', async function() {
      passwordManager.data.leakedCredentials =
          [autofill_test_util.makeCompromisedCredential(
              'google.com', 'jdoerrie', 'LEAKED')];
      passwordManager.plaintextPassword_ = 'test4';
      const checkPasswordSection = createCheckPasswordSection();

      await passwordManager.whenCalled('getCompromisedCredentials');

      Polymer.dom.flush();
      const listElements = checkPasswordSection.$.leakedPasswordList;
      const node = listElements.children[1];
      assertEquals('password', node.$.leakedPassword.type);
      assertNotEquals('test4', node.$.leakedPassword.value);

      // Open the more actions menu and click 'Show Password'.
      node.$.more.click();
      checkPasswordSection.$.menuShowPassword.click();

      const interaction =
          await passwordManager.whenCalled('recordPasswordCheckInteraction');

      assertEquals(
          PasswordManagerProxy.PasswordCheckInteraction.SHOW_PASSWORD,
          interaction);
      const {reason} =
          await passwordManager.whenCalled('getPlaintextCompromisedPassword');
      expectEquals(chrome.passwordsPrivate.PlaintextReason.VIEW, reason);
      assertEquals('text', node.$.leakedPassword.type);
      assertEquals('test4', node.$.leakedPassword.value);

      // Open the more actions menu and click 'Hide Password'.
      node.$.more.click();
      checkPasswordSection.$.menuShowPassword.click();

      assertEquals('password', node.$.leakedPassword.type);
      assertNotEquals('test4', node.$.leakedPassword.value);
    });

    // Verify if getPlaintext fails password will not be shown
    test('showHidePasswordMenuItemFail', async function() {
      passwordManager.data.leakedCredentials =
          [autofill_test_util.makeCompromisedCredential(
              'google.com', 'jdoerrie', 'LEAKED')];
      const checkPasswordSection = createCheckPasswordSection();
      await passwordManager.whenCalled('getCompromisedCredentials');

      Polymer.dom.flush();
      const listElements = checkPasswordSection.$.leakedPasswordList;
      const node = listElements.children[1];
      assertEquals('password', node.$.leakedPassword.type);
      assertNotEquals('test4', node.$.leakedPassword.value);

      // Open the more actions menu and click 'Show Password'.
      node.$.more.click();
      checkPasswordSection.$.menuShowPassword.click();
      await passwordManager.whenCalled('getPlaintextCompromisedPassword');
      // Verify that password field didn't change
      assertEquals('password', node.$.leakedPassword.type);
      assertNotEquals('test4', node.$.leakedPassword.value);
    });
  });
  // #cr_define_end
});
