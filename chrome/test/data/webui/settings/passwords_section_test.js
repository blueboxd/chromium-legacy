// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password Settings tests. */

// clang-format off
import {isChromeOS, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getToastManager} from 'chrome://settings/lazy_load.js';
import {MultiStoreExceptionEntry, MultiStorePasswordUiEntry, PasswordManagerImpl, PasswordManagerProxy, ProfileInfoBrowserProxyImpl, Router, routes, SettingsPluralStringProxyImpl} from 'chrome://settings/settings.js';
import {createExceptionEntry, createMultiStoreExceptionEntry, createMultiStorePasswordEntry, createPasswordEntry, makeCompromisedCredential, makePasswordCheckStatus, PasswordSectionElementFactory} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';
import {runCancelExportTest, runExportFlowErrorRetryTest, runExportFlowErrorTest, runExportFlowFastTest, runExportFlowSlowTest, runFireCloseEventAfterExportCompleteTest,runStartExportTest} from 'chrome://test/settings/passwords_export_test.js';
import {getSyncAllPrefs, simulateStoredAccounts, simulateSyncStatus} from 'chrome://test/settings/sync_test_util.m.js';
import {TestPasswordManagerProxy} from 'chrome://test/settings/test_password_manager_proxy.js';
import {TestProfileInfoBrowserProxy} from 'chrome://test/settings/test_profile_info_browser_proxy.m.js';
import {TestPluralStringProxy} from 'chrome://test/test_plural_string_proxy.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

// clang-format on

const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

/**
 * Helper method that validates a that elements in the password list match
 * the expected data.
 * @param {!Element} passwordsSection The passwords section element that will
 *     be checked.
 * @param {!Array<!MultiStorePasswordUiEntry>} expectedPasswords The expected
 *     data.
 * @private
 */
function validateMultiStorePasswordList(passwordsSection, expectedPasswords) {
  assertDeepEquals(expectedPasswords, passwordsSection.$.passwordList.items);
  const listItems =
      passwordsSection.shadowRoot.querySelectorAll('password-list-item');
  for (let index = 0; index < expectedPasswords.length; ++index) {
    const expected = expectedPasswords[index];
    const listItem = listItems[index];
    assertTrue(!!listItem);
    assertEquals(expected.urls.shown, listItem.$.originUrl.textContent.trim());
    assertEquals(expected.urls.link, listItem.$.originUrl.href);
    assertEquals(expected.username, listItem.$.username.value);
  }
}

/**
 * Convenience version of validateMultiStorePasswordList() for when store
 * duplicates don't exist.
 * @param {!Element} passwordsSection The passwords section element that will
 *     be checked.
 * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList The
 *     expected data.
 * @private
 */
function validatePasswordList(passwordsSection, passwordList) {
  validateMultiStorePasswordList(
      passwordsSection,
      passwordList.map(entry => new MultiStorePasswordUiEntry(entry)));
}


/**
 * Helper method that validates a that elements in the exception list match
 * the expected data.
 * @param {!Array<!Element>} nodes The nodes that will be checked.
 * @param {!Array<!MultiStoreExceptionEntry>} exceptionList The expected data.
 * @private
 */
function validateMultiStoreExceptionList(nodes, exceptionList) {
  assertEquals(exceptionList.length, nodes.length);
  for (let index = 0; index < exceptionList.length; ++index) {
    const node = nodes[index];
    const exception = exceptionList[index];
    assertEquals(
        exception.urls.shown,
        node.querySelector('#exception').textContent.trim());
    assertEquals(
        exception.urls.link.toLowerCase(),
        node.querySelector('#exception').href);
  }
}

/**
 * Convenience version of validateMultiStoreExceptionList() for when store
 * duplicates do not exist.
 * @param {!Array<!Element>} nodes The nodes that will be checked.
 * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList The
 *     expected data.
 * @private
 */
function validateExceptionList(nodes, exceptionList) {
  validateMultiStoreExceptionList(
      nodes, exceptionList.map(entry => new MultiStoreExceptionEntry(entry)));
}

/**
 * Returns all children of an element that has children added by a dom-repeat.
 * @param {!Element} element
 * @return {!Array<!Element>}
 * @private
 */
function getDomRepeatChildren(element) {
  const nodes = element.querySelectorAll('.list-item:not([id])');
  return nodes;
}

/**
 * Extracts the first password-list-item in the a password-section element.
 * @param {!Element} passwordsSection
 */
function getFirstPasswordListItem(passwordsSection) {
  // The first child is a template, skip and get the real 'first child'.
  return passwordsSection.$$('password-list-item');
}

/**
 * Helper method used to test for a url in a list of passwords.
 * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
 * @param {string} url The URL that is being searched for.
 */
function listContainsUrl(passwordList, url) {
  for (let i = 0; i < passwordList.length; ++i) {
    if (passwordList[i].urls.origin === url) {
      return true;
    }
  }
  return false;
}

/**
 * Helper method used to test for a url in a list of passwords.
 * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
 * @param {string} url The URL that is being searched for.
 */
function exceptionsListContainsUrl(exceptionList, url) {
  for (let i = 0; i < exceptionList.length; ++i) {
    if (exceptionList[i].urls.orginUrl === url) {
      return true;
    }
  }
  return false;
}

/**
 * Simulates user who is eligible and opted-in for account storage. Should be
 * called after the PasswordsSection element is created. The load time value for
 * enableAccountStorage must be overridden separately.
 * @param {TestPasswordManagerProxy} passwordManager
 */
function simulateAccountStorageUser(passwordManager) {
  simulateSyncStatus({signedIn: false});
  simulateStoredAccounts([{
    fullName: 'john doe',
    givenName: 'john',
    email: 'john@gmail.com',
  }]);
  passwordManager.setIsOptedInForAccountStorageAndNotify(true);
}

suite('PasswordsSection', function() {
  /** @type {TestPasswordManagerProxy} */
  let passwordManager = null;

  /** @type {PasswordSectionElementFactory} */
  let elementFactory = null;

  /** @type {TestPluralStringProxy} */
  let pluralString = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({enablePasswordCheck: true});
  });

  setup(function() {
    PolymerTest.clearBody();
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    pluralString = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.instance_ = pluralString;

    PasswordManagerImpl.instance_ = passwordManager;
    elementFactory = new PasswordSectionElementFactory(document);
  });

  test('testPasswordsExtensionIndicator', function() {
    // Initialize with dummy prefs.
    const element = document.createElement('passwords-section');
    element.prefs = {
      credentials_enable_service: {},
    };
    document.body.appendChild(element);

    assertFalse(!!element.$$('#passwordsExtensionIndicator'));
    element.set('prefs.credentials_enable_service.extensionId', 'test-id');
    flush();

    assertTrue(!!element.$$('#passwordsExtensionIndicator'));
  });

  test('verifyNoSavedPasswords', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);

    validatePasswordList(passwordsSection, []);

    assertFalse(passwordsSection.$.noPasswordsLabel.hidden);
    assertTrue(passwordsSection.$.savedPasswordsHeaders.hidden);
  });

  test('verifySavedPasswordEntries', function() {
    const passwordList = [
      createPasswordEntry({url: 'site1.com', username: 'luigi', id: 0}),
      createPasswordEntry({url: 'longwebsite.com', username: 'peach', id: 1}),
      createPasswordEntry({url: 'site2.com', username: 'mario', id: 2}),
      createPasswordEntry({url: 'site1.com', username: 'peach', id: 3}),
      createPasswordEntry({url: 'google.com', username: 'mario', id: 4}),
      createPasswordEntry({url: 'site2.com', username: 'luigi', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    // Assert that the data is passed into the iron list. If this fails,
    // then other expectations will also fail.
    assertDeepEquals(
        passwordList.map(entry => new MultiStorePasswordUiEntry(entry)),
        passwordsSection.$.passwordList.items);

    validatePasswordList(passwordsSection, passwordList);

    assertTrue(passwordsSection.$.noPasswordsLabel.hidden);
    assertFalse(passwordsSection.$.savedPasswordsHeaders.hidden);
  });

  // Test verifies that passwords duplicated across stores get properly merged
  // in the UI.
  test('verifySavedPasswordEntriesWithMultiStore', function() {
    // Entries with duplicates.
    const accountPassword1 = createPasswordEntry(
        {username: 'user1', frontendId: 1, id: 10, fromAccountStore: true});
    const devicePassword1 = createPasswordEntry(
        {username: 'user1', frontendId: 1, id: 11, fromAccountStore: false});
    const accountPassword2 = createPasswordEntry(
        {username: 'user2', frontendId: 2, id: 20, fromAccountStore: true});
    const devicePassword2 = createPasswordEntry(
        {username: 'user2', frontendId: 2, id: 21, fromAccountStore: false});
    // Entries without duplicate.
    const devicePassword3 = createPasswordEntry(
        {username: 'user3', frontendId: 3, id: 3, fromAccountStore: false});
    const accountPassword4 = createPasswordEntry(
        {username: 'user4', frontendId: 4, id: 4, fromAccountStore: true});

    // Shuffle entries a little.
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager,
        [
          devicePassword3, accountPassword1, devicePassword2, accountPassword4,
          devicePassword1, accountPassword2
        ],
        []);

    // Expected list keeping relative order.
    const expectedList = [
      createMultiStorePasswordEntry({username: 'user3', deviceId: 3}),
      createMultiStorePasswordEntry(
          {username: 'user1', accountId: 10, deviceId: 11}),
      createMultiStorePasswordEntry(
          {username: 'user2', accountId: 20, deviceId: 21}),
      createMultiStorePasswordEntry({username: 'user4', accountId: 4}),
    ];

    validateMultiStorePasswordList(passwordsSection, expectedList);
  });

  // Test verifies that removing a password will update the elements.
  test('verifyPasswordListRemove', function() {
    const passwordList = [
      createPasswordEntry(
          {url: 'anotherwebsite.com', username: 'luigi', id: 0}),
      createPasswordEntry({url: 'longwebsite.com', username: 'peach', id: 1}),
      createPasswordEntry({url: 'website.com', username: 'mario', id: 2})
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    // Simulate 'longwebsite.com' being removed from the list.
    passwordList.splice(1, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    assertFalse(
        listContainsUrl(passwordsSection.savedPasswords, 'longwebsite.com'));
    assertFalse(listContainsUrl(passwordList, 'longwebsite.com'));

    validatePasswordList(passwordsSection, passwordList);
  });

  // Test verifies that removing the account copy of a duplicated password will
  // still leave the device copy present.
  test('verifyPasswordListRemoveAccountCopy', function() {
    const passwordList = [
      createPasswordEntry({frontendId: 0, id: 0, fromAccountStore: true}),
      createPasswordEntry({frontendId: 0, id: 1, fromAccountStore: false}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validateMultiStorePasswordList(
        passwordsSection,
        [createMultiStorePasswordEntry({accountId: 0, deviceId: 1})]);
    // Simulate account copy being removed from the list.
    passwordList.splice(0, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validateMultiStorePasswordList(
        passwordsSection, [createMultiStorePasswordEntry({deviceId: 1})]);
  });

  // Test verifies that removing the device copy of a duplicated password will
  // still leave the account copy present.
  test('verifyPasswordListRemoveDeviceCopy', function() {
    const passwordList = [
      createPasswordEntry({frontendId: 0, id: 0, fromAccountStore: true}),
      createPasswordEntry({frontendId: 0, id: 1, fromAccountStore: false}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validateMultiStorePasswordList(
        passwordsSection,
        [createMultiStorePasswordEntry({accountId: 0, deviceId: 1})]);
    // Simulate device copy being removed from the list.
    passwordList.splice(1, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validateMultiStorePasswordList(
        passwordsSection, [createMultiStorePasswordEntry({accountId: 0})]);
  });

  // Test verifies that removing both copies of a duplicated password will
  // cause no password to be displayed.
  test('verifyPasswordListRemoveBothCopies', function() {
    const passwordList = [
      createPasswordEntry({frontendId: 0, id: 0, fromAccountStore: true}),
      createPasswordEntry({frontendId: 0, id: 1, fromAccountStore: false}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validateMultiStorePasswordList(
        passwordsSection,
        [createMultiStorePasswordEntry({accountId: 0, deviceId: 1})]);
    // Simulate both copies being removed from the list.
    passwordList.splice(0, 2);
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validateMultiStorePasswordList(passwordsSection, []);
  });

  // Test verifies that adding a password will update the elements.
  test('verifyPasswordListAdd', function() {
    const passwordList = [
      createPasswordEntry(
          {url: 'anotherwebsite.com', username: 'luigi', id: 0}),
      createPasswordEntry({url: 'longwebsite.com', username: 'peach', id: 1}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    // Simulate 'website.com' being added to the list.
    passwordList.unshift(
        createPasswordEntry({url: 'website.com', username: 'mario', id: 2}));
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validatePasswordList(passwordsSection, passwordList);
  });

  // Test verifies that adding an account copy of an existing password will
  // merge it with the one already in the list.
  test('verifyPasswordListAddAccountCopy', function() {
    const passwordList = [
      createPasswordEntry({frontendId: 0, fromAccountStore: false, id: 0}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    // Simulate account copy being added to the list.
    passwordList.unshift(
        createPasswordEntry({frontendId: 0, fromAccountStore: true, id: 1}));

    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validateMultiStorePasswordList(
        passwordsSection,
        [createMultiStorePasswordEntry({deviceId: 0, accountId: 1})]);
  });

  // Test verifies that adding a device copy of an existing password will
  // merge it with the one already in the list.
  test('verifyPasswordListAddDeviceCopy', function() {
    const passwordList = [
      createPasswordEntry({frontendId: 0, fromAccountStore: true, id: 0}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    // Simulate device copy being added to the list.
    passwordList.unshift(
        createPasswordEntry({frontendId: 0, fromAccountStore: false, id: 1}));

    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validateMultiStorePasswordList(
        passwordsSection,
        [createMultiStorePasswordEntry({accountId: 0, deviceId: 1})]);
  });

  // Test verifies that removing one out of two passwords for the same website
  // will update the elements.
  test('verifyPasswordListRemoveSameWebsite', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);

    // Set-up initial list.
    let passwordList = [
      createPasswordEntry({url: 'website.com', username: 'mario', id: 0}),
      createPasswordEntry({url: 'website.com', username: 'luigi', id: 1})
    ];

    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();
    validatePasswordList(passwordsSection, passwordList);

    // Simulate '(website.com, mario)' being removed from the list.
    passwordList.shift();
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();
    validatePasswordList(passwordsSection, passwordList);

    // Simulate '(website.com, luigi)' being removed from the list as well.
    passwordList = [];
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();
    validatePasswordList(passwordsSection, passwordList);
  });

  // Test verifies that pressing the 'remove' button will trigger a remove
  // event. Does not actually remove any passwords.
  test('verifyPasswordItemRemoveButton', async function() {
    const passwordList = [
      createPasswordEntry({url: 'one', username: 'six', id: 0}),
      createPasswordEntry({url: 'two', username: 'five', id: 1}),
      createPasswordEntry({url: 'three', username: 'four', id: 2}),
      createPasswordEntry({url: 'four', username: 'three', id: 3}),
      createPasswordEntry({url: 'five', username: 'two', id: 4}),
      createPasswordEntry({url: 'six', username: 'one', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    const firstNode = getFirstPasswordListItem(passwordsSection);
    assertTrue(!!firstNode);
    const firstPassword = passwordList[0];

    // Click the remove button on the first password.
    firstNode.$$('#passwordMenu').click();
    passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();

    const id = await passwordManager.whenCalled('removeSavedPassword');
    // Verify that the expected value was passed to the proxy.
    assertEquals(firstPassword.id, id);
    assertEquals(
        passwordsSection.i18n('passwordDeleted'),
        getToastManager().$.content.textContent);
  });

  // Test verifies that 'Copy password' button is hidden for Federated
  // (passwordless) credentials. Does not test Copy button.
  test('verifyCopyAbsentForFederatedPasswordInMenu', function() {
    const passwordList = [
      createPasswordEntry({federationText: 'with chromium.org'}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    flush();

    getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
    assertTrue(
        passwordsSection.$.passwordsListHandler.$$('#menuCopyPassword').hidden);
  });

  // Test verifies that 'Copy password' button is not hidden for common
  // credentials. Does not test Copy button.
  test('verifyCopyPresentInMenu', function() {
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'hey'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    flush();

    getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
    assertFalse(
        passwordsSection.$.passwordsListHandler.$$('#menuCopyPassword').hidden);
  });

  test('verifyFilterPasswords', function() {
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'three.com/show', username: 'four', id: 2}),
      createPasswordEntry({url: 'four.com', username: 'three', id: 3}),
      createPasswordEntry({url: 'five.com', username: 'two', id: 4}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    passwordsSection.filter = 'SHow';
    flush();

    const expectedList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'three.com/show', username: 'four', id: 2}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    validatePasswordList(passwordsSection, expectedList);
  });

  test('verifyFilterPasswordsWithRemoval', function() {
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'three.com/show', username: 'four', id: 2}),
      createPasswordEntry({url: 'four.com', username: 'three', id: 3}),
      createPasswordEntry({url: 'five.com', username: 'two', id: 4}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    passwordsSection.filter = 'SHow';
    flush();

    let expectedList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'three.com/show', username: 'four', id: 2}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    validatePasswordList(passwordsSection, expectedList);

    // Simulate removal of three.com/show
    passwordList.splice(2, 1);

    expectedList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();
    validatePasswordList(passwordsSection, expectedList);
  });

  test('verifyFilterPasswordExceptions', function() {
    const exceptionList = [
      createExceptionEntry({url: 'docsshoW.google.com', id: 0}),
      createExceptionEntry({url: 'showmail.com', id: 1}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'mapsshow.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.comshow', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], exceptionList);
    passwordsSection.filter = 'shOW';
    flush();

    const expectedExceptionList = [
      createExceptionEntry({url: 'docsshoW.google.com', id: 0}),
      createExceptionEntry({url: 'showmail.com', id: 1}),
      createExceptionEntry({url: 'mapsshow.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.comshow', id: 5}),
    ];

    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        expectedExceptionList);
  });

  test('verifyNoPasswordExceptions', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);

    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList), []);

    assertFalse(passwordsSection.$.noExceptionsLabel.hidden);
  });

  test('verifyPasswordExceptions', function() {
    const exceptionList = [
      createExceptionEntry({url: 'docs.google.com', id: 0}),
      createExceptionEntry({url: 'mail.com', id: 1}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'maps.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.com', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], exceptionList);

    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        exceptionList);

    assertTrue(passwordsSection.$.noExceptionsLabel.hidden);
  });

  // Test verifies that exceptions duplicated across stores get properly merged
  // in the UI.
  test('verifyPasswordExceptionsWithMultiStore', function() {
    // Entries with duplicates.
    const accountException1 = createExceptionEntry(
        {url: '1.com', frontendId: 1, id: 10, fromAccountStore: true});
    const deviceException1 = createExceptionEntry(
        {url: '1.com', frontendId: 1, id: 11, fromAccountStore: false});
    const accountException2 = createExceptionEntry(
        {url: '2.com', frontendId: 2, id: 20, fromAccountStore: true});
    const deviceException2 = createExceptionEntry(
        {url: '2.com', frontendId: 2, id: 21, fromAccountStore: false});
    // Entries without duplicate.
    const deviceException3 = createExceptionEntry(
        {url: '3.com', frontendId: 3, id: 3, fromAccountStore: false});
    const accountException4 = createExceptionEntry(
        {url: '4.com', frontendId: 4, id: 4, fromAccountStore: true});

    // Shuffle entries a little.
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], [
          deviceException3, accountException1, deviceException2,
          accountException4, deviceException1, accountException2
        ]);

    // Expected list keeping relative order.
    const expectedList = [
      createMultiStoreExceptionEntry({url: '3.com', deviceId: 3}),
      createMultiStoreExceptionEntry(
          {url: '1.com', accountId: 10, deviceId: 11}),
      createMultiStoreExceptionEntry(
          {url: '2.com', accountId: 20, deviceId: 21}),
      createMultiStoreExceptionEntry({url: '4.com', accountId: 4}),
    ];

    validateMultiStoreExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        expectedList);

    assertTrue(passwordsSection.$.noExceptionsLabel.hidden);
  });

  // Test verifies that removing an exception will update the elements.
  test('verifyPasswordExceptionRemove', function() {
    const exceptionList = [
      createExceptionEntry({url: 'docs.google.com', id: 0}),
      createExceptionEntry({url: 'mail.com', id: 1}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'maps.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.com', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], exceptionList);

    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        exceptionList);

    // Simulate 'mail.com' being removed from the list.
    passwordsSection.splice('passwordExceptions', 1, 1);
    assertFalse(exceptionsListContainsUrl(
        passwordsSection.passwordExceptions, 'mail.com'));
    assertFalse(exceptionsListContainsUrl(exceptionList, 'mail.com'));
    flush();

    const expectedExceptionList = [
      createExceptionEntry({url: 'docs.google.com', id: 0}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'maps.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.com', id: 5})
    ];
    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        expectedExceptionList);
  });

  // Test verifies that pressing the 'remove' button will trigger a remove
  // event. Does not actually remove any exceptions.
  test('verifyPasswordExceptionRemoveButton', function() {
    const exceptionList = [
      createExceptionEntry({url: 'docs.google.com', id: 0}),
      createExceptionEntry({url: 'mail.com', id: 1}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'maps.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.com', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], exceptionList);

    const exceptions =
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList);

    // The index of the button currently being checked.
    let item = 0;

    const clickRemoveButton = function() {
      exceptions[item].querySelector('#removeExceptionButton').click();
    };

    // Removes the next exception item, verifies that the expected method was
    // called on |passwordManager| and continues recursively until no more items
    // exist.
    function removeNextRecursive() {
      passwordManager.resetResolver('removeExceptions');
      clickRemoveButton();
      return passwordManager.whenCalled('removeExceptions').then(ids => {
        // Verify that the event matches the expected value.
        assertTrue(item < exceptionList.length);
        assertDeepEquals(ids, [exceptionList[item].id]);

        if (++item < exceptionList.length) {
          return removeNextRecursive();
        }
      });
    }

    // Click 'remove' on all passwords, one by one.
    return removeNextRecursive();
  });

  // Test verifies that pressing the 'remove' button for a duplicated exception
  // will remove both the device and account copies.
  test('verifyDuplicatedExceptionRemoveButton', async function() {
    // Create a duplicated exception that will be merged into a single entry.
    const deviceCopy =
        createPasswordEntry({frontendId: 42, id: 0, fromAccountStore: false});
    const accountCopy =
        createPasswordEntry({frontendId: 42, id: 1, fromAccountStore: true});

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], [deviceCopy, accountCopy]);

    const [mergedEntry] =
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList);
    mergedEntry.querySelector('#removeExceptionButton').click();

    // Verify both ids get passed to the proxy.
    const ids = await passwordManager.whenCalled('removeExceptions');
    assertTrue(ids.includes(deviceCopy.id));
    assertTrue(ids.includes(accountCopy.id));
  });

  test('verifyFederatedPassword', function() {
    const item = createMultiStorePasswordEntry(
        {federationText: 'with chromium.org', username: 'bart', deviceId: 42});
    const passwordDialog = elementFactory.createPasswordEditDialog(item);

    flush();

    assertEquals(item.federationText, passwordDialog.$.passwordInput.value);
    // Text should be readable.
    assertEquals('text', passwordDialog.$.passwordInput.type);
    assertTrue(passwordDialog.$.showPasswordButton.hidden);
  });

  // Test verifies that the edit dialog informs the password is stored in the
  // account.
  test('verifyStorageDetailsInEditDialogForAccountPassword', function() {
    const accountPassword = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', accountId: 42});
    const accountPasswordDialog =
        elementFactory.createPasswordEditDialog(accountPassword);
    flush();

    // By default no message is displayed.
    assertTrue(accountPasswordDialog.$.storageDetails.hidden);

    // Display the message.
    accountPasswordDialog.shouldShowStorageDetails = true;
    flush();
    assertFalse(accountPasswordDialog.$.storageDetails.hidden);
    assertEquals(
        accountPasswordDialog.i18n('passwordStoredInAccount'),
        accountPasswordDialog.$.storageDetails.innerText);
  });

  // Test verifies that the edit dialog informs the password is stored on the
  // device.
  test('verifyStorageDetailsInEditDialogForDevicePassword', function() {
    const devicePassword = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', deviceId: 42});
    const devicePasswordDialog =
        elementFactory.createPasswordEditDialog(devicePassword);
    flush();

    // By default no message is displayed.
    assertTrue(devicePasswordDialog.$.storageDetails.hidden);

    // Display the message.
    devicePasswordDialog.shouldShowStorageDetails = true;
    flush();
    assertFalse(devicePasswordDialog.$.storageDetails.hidden);
    assertEquals(
        devicePasswordDialog.i18n('passwordStoredOnDevice'),
        devicePasswordDialog.$.storageDetails.innerText);
  });

  // Test verifies that the edit dialog informs the password is stored both on
  // the device and in the account.
  test(
      'verifyStorageDetailsInEditDialogForPasswordInBothLocations', function() {
        const accountAndDevicePassword = createMultiStorePasswordEntry(
            {url: 'goo.gl', username: 'bart', deviceId: 42, accountId: 43});
        const accountAndDevicePasswordDialog =
            elementFactory.createPasswordEditDialog(accountAndDevicePassword);
        flush();

        // By default no message is displayed.
        assertTrue(accountAndDevicePasswordDialog.$.storageDetails.hidden);

        // Display the message.
        accountAndDevicePasswordDialog.shouldShowStorageDetails = true;
        flush();
        assertFalse(accountAndDevicePasswordDialog.$.storageDetails.hidden);
        assertEquals(
            accountAndDevicePasswordDialog.i18n(
                'passwordStoredInAccountAndOnDevice'),
            accountAndDevicePasswordDialog.$.storageDetails.innerText);
      });

  test('showSavedPasswordEditDialog', function() {
    const PASSWORD = 'bAn@n@5';
    const item = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', deviceId: 42});
    const passwordDialog = elementFactory.createPasswordEditDialog(item);

    assertFalse(passwordDialog.$.showPasswordButton.hidden);

    passwordDialog.password = PASSWORD;
    flush();

    assertEquals(PASSWORD, passwordDialog.$.passwordInput.value);
    // Password should be visible.
    assertEquals('text', passwordDialog.$.passwordInput.type);
    assertFalse(passwordDialog.$.showPasswordButton.hidden);
  });

  test('showSavedPasswordListItem', function() {
    const PASSWORD = 'bAn@n@5';
    const item = createPasswordEntry({url: 'goo.gl', username: 'bart'});
    const passwordListItem = elementFactory.createPasswordListItem(item);
    // Hidden passwords should be disabled.
    assertTrue(passwordListItem.$$('#password').disabled);

    passwordListItem.password = PASSWORD;
    flush();

    assertEquals(PASSWORD, passwordListItem.$$('#password').value);
    // Password should be visible.
    assertEquals('text', passwordListItem.$$('#password').type);
    // Visible passwords should not be disabled.
    assertFalse(passwordListItem.$$('#password').disabled);

    // Hide Password Button should be shown.
    assertTrue(passwordListItem.$$('#showPasswordButton')
                   .classList.contains('icon-visibility-off'));
  });

  // Tests that invoking the plaintext password sets the corresponding
  // password.
  test('onShowSavedPasswordEditDialog', function() {
    const expectedItem = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', deviceId: 1});
    const passwordDialog =
        elementFactory.createPasswordEditDialog(expectedItem);
    assertEquals('', passwordDialog.password);

    passwordManager.setPlaintextPassword('password');
    passwordDialog.$.showPasswordButton.click();
    return passwordManager.whenCalled('requestPlaintextPassword')
        .then(({id, reason}) => {
          assertEquals(1, id);
          assertEquals('VIEW', reason);
          assertEquals('password', passwordDialog.password);
        });
  });

  test('onShowSavedPasswordListItem', function() {
    const expectedItem =
        createPasswordEntry({url: 'goo.gl', username: 'bart', id: 1});
    const passwordListItem =
        elementFactory.createPasswordListItem(expectedItem);
    assertEquals('', passwordListItem.password);

    passwordManager.setPlaintextPassword('password');
    passwordListItem.$$('#showPasswordButton').click();
    return passwordManager.whenCalled('requestPlaintextPassword')
        .then(({id, reason}) => {
          assertEquals(1, id);
          assertEquals('VIEW', reason);
          assertEquals('password', passwordListItem.password);
        });
  });

  test('onCopyPasswordListItem', function() {
    const expectedItem =
        createPasswordEntry({url: 'goo.gl', username: 'bart', id: 1});
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [expectedItem], []);

    getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
    passwordsSection.$.passwordsListHandler.$$('#menuCopyPassword').click();

    return passwordManager.whenCalled('requestPlaintextPassword')
        .then(({id, reason}) => {
          assertEquals(1, id);
          assertEquals('COPY', reason);
        });
  });

  test('closingPasswordsSectionHidesUndoToast', function(done) {
    const passwordEntry =
        createPasswordEntry({url: 'goo.gl', username: 'bart'});
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [passwordEntry], []);
    const toastManager = getToastManager();

    // Click the remove button on the first password and assert that an undo
    // toast is shown.
    getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
    passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
    assertTrue(toastManager.isToastOpen);

    // Remove the passwords section from the DOM and check that this closes
    // the undo toast.
    document.body.removeChild(passwordsSection);
    assertFalse(toastManager.isToastOpen);

    done();
  });

  // Chrome offers the export option when there are passwords.
  test('offerExportWhenPasswords', function(done) {
    const passwordList = [
      createPasswordEntry({url: 'googoo.com', username: 'Larry'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    assertFalse(passwordsSection.$.menuExportPassword.hidden);
    done();
  });

  // Chrome shouldn't offer the option to export passwords if there are no
  // passwords.
  test('noExportIfNoPasswords', function(done) {
    const passwordList = [];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    assertTrue(passwordsSection.$.menuExportPassword.hidden);
    done();
  });

  // Test that clicking the Export Passwords menu item opens the export
  // dialog.
  test('exportOpen', function(done) {
    const passwordList = [
      createPasswordEntry({url: 'googoo.com', username: 'Larry'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    // The export dialog calls requestExportProgressStatus() when opening.
    passwordManager.requestExportProgressStatus = (callback) => {
      callback(chrome.passwordsPrivate.ExportProgressStatus.NOT_STARTED);
      done();
    };
    passwordManager.addPasswordsFileExportProgressListener = () => {};
    passwordsSection.$.menuExportPassword.click();
  });

  if (!isChromeOS) {
    // Test that tapping "Export passwords..." notifies the browser.
    test('startExport', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      runStartExportTest(exportDialog, passwordManager, done);
    });

    // Test the export flow. If exporting is fast, we should skip the
    // in-progress view altogether.
    test('exportFlowFast', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      runExportFlowFastTest(exportDialog, passwordManager, done);
    });

    // The error view is shown when an error occurs.
    test('exportFlowError', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      runExportFlowErrorTest(exportDialog, passwordManager, done);
    });

    // The error view allows to retry.
    test('exportFlowErrorRetry', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      runExportFlowErrorRetryTest(exportDialog, passwordManager, done);
    });

    // Test the export flow. If exporting is slow, Chrome should show the
    // in-progress dialog for at least 1000ms.
    test('exportFlowSlow', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      runExportFlowSlowTest(exportDialog, passwordManager, done);
    });

    // Test that canceling the dialog while exporting will also cancel the
    // export on the browser.
    test('cancelExport', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      runCancelExportTest(exportDialog, passwordManager, done);
    });

    test('fires close event after export complete', () => {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      return runFireCloseEventAfterExportCompleteTest(
          exportDialog, passwordManager);
    });

    // Test verifies that the overflow menu does not offer an option to move a
    // password to the account.
    test('noMoveToAccountOption', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      assertFalse(!!passwordsSection.$.passwordsListHandler.$$(
          '#menuMovePasswordToAccount'));
    });

    // Tests that the opt-in/opt-out buttons appear for signed-in (non-sync)
    // users and that the description changes accordingly.
    test('changeOptInButtonsBasedOnSignInAndAccountStorageOptIn', function() {
      // Feature flag enabled.
      loadTimeData.overrideValues({enableAccountStorage: true});

      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      // Sync is disabled and the user is initially signed out.
      simulateSyncStatus({signedIn: false});
      const isDisplayed = element => !!element && !element.hidden;
      assertFalse(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));

      // User signs in but is not opted in yet.
      simulateStoredAccounts([{
        fullName: 'john doe',
        givenName: 'john',
        email: 'john@gmail.com',
      }]);
      passwordManager.setIsOptedInForAccountStorageAndNotify(false);
      assertTrue(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));
      assertTrue(isDisplayed(passwordsSection.$.optInToAccountStorageButton));
      assertFalse(isDisplayed(passwordsSection.$.optOutOfAccountStorageButton));
      assertTrue(isDisplayed(passwordsSection.$.accountStorageOptInBody));
      assertFalse(isDisplayed(passwordsSection.$.accountStorageOptOutBody));

      // Opt in.
      passwordManager.setIsOptedInForAccountStorageAndNotify(true);
      assertTrue(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));
      assertFalse(isDisplayed(passwordsSection.$.optInToAccountStorageButton));
      assertTrue(isDisplayed(passwordsSection.$.optOutOfAccountStorageButton));
      assertTrue(isDisplayed(passwordsSection.$.accountStorageOptOutBody));
      assertFalse(isDisplayed(passwordsSection.$.accountStorageOptInBody));

      // Sign out
      simulateStoredAccounts([]);
      assertFalse(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));
    });

    // Tests that profile picture and account mail address are shown for the
    // opt-in buttons.
    test('showAccountImageAndEmailOnOptInButtons', function() {
      // Create fake profile data.
      const profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
      ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;
      const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
          'LAAAAAABAAEAAAICTAEAOw==';

      // Feature flag enabled.
      loadTimeData.overrideValues({enableAccountStorage: true});

      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      // Sync is disabled and the user is initially signed out.
      simulateSyncStatus({signedIn: false});
      const isDisplayed = element => !!element && !element.hidden;
      assertFalse(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));

      // User signs in which updates the profile info.
      simulateStoredAccounts([{
        fullName: 'john doe',
        givenName: 'john',
        email: 'john@gmail.com',
      }]);
      webUIListenerCallback(
          'profile-info-changed', {name: 'john doe', iconUrl: iconDataUrl});
      flush();

      passwordManager.setIsOptedInForAccountStorageAndNotify(false);
      assertEquals('john@gmail.com', passwordsSection.$.accountEmail.innerText);
      const bg = passwordsSection.$.profileIcon.style.backgroundImage;
      assertTrue(bg.includes(iconDataUrl));
    });

    // Test verifies that enabling sync hides the buttons for account storage
    // opt-in/out and the 'device passwords' page.
    test('enablingSyncHidesAccountStorageButtons', function() {
      // Feature flag enabled.
      loadTimeData.overrideValues({enableAccountStorage: true});

      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      simulateSyncStatus({signedIn: false});
      simulateStoredAccounts([{
        fullName: 'john doe',
        givenName: 'john',
        email: 'john@gmail.com',
      }]);
      passwordManager.setIsOptedInForAccountStorageAndNotify(true);

      const isDisplayed = element => !!element && !element.hidden;
      assertTrue(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));

      // Enable sync.
      simulateSyncStatus({signedIn: true});
      assertFalse(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));
    });

    // Test verifies that the button linking to the 'device passwords' page is
    // only visible when there is at least one device password, and that it has
    // the appropriate text.
    test('verifyDevicePasswordsButtonVisibility', function() {
      // Set up user eligible to the account-scoped password storage, not
      // opted in and with no device passwords. Button should be hidden.
      loadTimeData.overrideValues({enableAccountStorage: true});
      const passwordList =
          [createPasswordEntry({fromAccountStore: true, id: 10})];
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      simulateSyncStatus({signedIn: false});
      simulateStoredAccounts([{
        fullName: 'john doe',
        givenName: 'john',
        email: 'john@gmail.com',
      }]);
      assertTrue(passwordsSection.$.devicePasswordsLink.hidden);

      // Opting in still doesn't display it because the user has no device
      // passwords yet.
      passwordManager.setIsOptedInForAccountStorageAndNotify(true);
      assertTrue(passwordsSection.$.devicePasswordsLink.hidden);

      // Add a device password. The button shows up, with the text in singular
      // form.
      passwordList.unshift(
          createPasswordEntry({fromAccountStore: false, id: 20}));
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      flush();
      assertFalse(passwordsSection.$.devicePasswordsLink.hidden);
      assertEquals(
          passwordsSection.i18n('devicePasswordsLinkLabelSingular'),
          passwordsSection.$.devicePasswordsLinkLabel.innerText);

      // Add a second device password. The text nows says '2 passwords'.
      passwordList.unshift(
          createPasswordEntry({fromAccountStore: false, id: 30}));
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      flush();
      assertFalse(passwordsSection.$.devicePasswordsLink.hidden);
      assertEquals(
          passwordsSection.i18n('devicePasswordsLinkLabelPlural', 2),
          passwordsSection.$.devicePasswordsLinkLabel.innerText);
    });

    // Test verifies that, for account-scoped password storage users, removing
    // a password stored in a single location indicates the location in the
    // toast manager message.
    test(
        'passwordRemovalMessageSpecifiesStoreForAccountStorageUsers',
        function() {
          loadTimeData.overrideValues({enableAccountStorage: true});

          const passwordList = [
            createPasswordEntry(
                {username: 'account', id: 0, fromAccountStore: true}),
            createPasswordEntry(
                {username: 'local', id: 1, fromAccountStore: false}),
          ];
          const passwordsSection = elementFactory.createPasswordsSection(
              passwordManager, passwordList, []);

          simulateAccountStorageUser(passwordManager);

          // No removal actually happens, so all passwords keep their position.
          const passwordListItems =
              passwordsSection.root.querySelectorAll('password-list-item');
          passwordListItems[0].$$('#passwordMenu').click();
          passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
          assertEquals(
              passwordsSection.i18n('passwordDeletedFromAccount'),
              getToastManager().$.content.textContent);

          passwordListItems[1].$$('#passwordMenu').click();
          passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
          assertEquals(
              passwordsSection.i18n('passwordDeletedFromDevice'),
              getToastManager().$.content.textContent);
        });

    // Test verifies that if the user attempts to remove a password stored
    // both on the device and in the account, the PasswordRemoveDialog shows up.
    // Clicking the button in the dialog then removes both versions of the
    // password.
    test('verifyPasswordRemoveDialogRemoveBothCopies', async function() {
      loadTimeData.overrideValues({enableAccountStorage: true});

      const accountCopy =
          createPasswordEntry({frontendId: 42, id: 0, fromAccountStore: true});
      const deviceCopy =
          createPasswordEntry({frontendId: 42, id: 1, fromAccountStore: false});
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [accountCopy, deviceCopy], []);

      simulateAccountStorageUser(passwordManager);

      // At first the dialog is not shown.
      assertTrue(
          !passwordsSection.$.passwordsListHandler.$$('#passwordRemoveDialog'));

      // Clicking remove in the overflow menu shows the dialog.
      getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
      passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
      flush();
      const removeDialog =
          passwordsSection.$.passwordsListHandler.$$('#passwordRemoveDialog');
      assertTrue(!!removeDialog);

      // Both checkboxes are selected by default. Confirming removes from both
      // account and device.
      assertTrue(
          removeDialog.$.removeFromAccountCheckbox.checked &&
          removeDialog.$.removeFromDeviceCheckbox.checked);
      removeDialog.$.removeButton.click();
      const removedIds =
          await passwordManager.whenCalled('removeSavedPasswords');
      assertTrue(removedIds.includes(accountCopy.id));
      assertTrue(removedIds.includes(deviceCopy.id));
    });

    // Test verifies that if the user attempts to remove a password stored
    // both on the device and in the account, the PasswordRemoveDialog shows up.
    // The user then chooses to remove only of the copies.
    test('verifyPasswordRemoveDialogRemoveSingleCopy', async function() {
      loadTimeData.overrideValues({enableAccountStorage: true});

      const accountCopy =
          createPasswordEntry({frontendId: 42, id: 0, fromAccountStore: true});
      const deviceCopy =
          createPasswordEntry({frontendId: 42, id: 1, fromAccountStore: false});
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [accountCopy, deviceCopy], []);

      simulateAccountStorageUser(passwordManager);

      // At first the dialog is not shown.
      assertTrue(
          !passwordsSection.$.passwordsListHandler.$$('#passwordRemoveDialog'));

      // Clicking remove in the overflow menu shows the dialog.
      getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
      passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
      flush();
      const removeDialog =
          passwordsSection.$.passwordsListHandler.$$('#passwordRemoveDialog');
      assertTrue(!!removeDialog);

      // Uncheck the account checkboxes then confirm. Only the device copy is
      // removed.
      removeDialog.$.removeFromAccountCheckbox.click();
      assertTrue(
          !removeDialog.$.removeFromAccountCheckbox.checked &&
          removeDialog.$.removeFromDeviceCheckbox.checked);
      removeDialog.$.removeButton.click();
      const removedIds =
          await passwordManager.whenCalled('removeSavedPasswords');
      assertTrue(removedIds.includes(deviceCopy.id));
    });
  }

  // The export dialog is dismissable.
  test('exportDismissable', function(done) {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager);

    assertTrue(exportDialog.$$('#dialog_start').open);
    exportDialog.$$('#cancelButton').click();
    flush();
    assertFalse(!!exportDialog.$$('#dialog_start'));

    done();
  });

  test('fires close event when canceled', () => {
    const exportDialog =
        elementFactory.createExportPasswordsDialog(passwordManager);
    const wait = eventToPromise('passwords-export-dialog-close', exportDialog);
    exportDialog.$$('#cancelButton').click();
    return wait;
  });

  test('hideLinkToPasswordManagerWhenEncrypted', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    const syncPrefs = getSyncAllPrefs();
    syncPrefs.encryptAllData = true;
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    simulateSyncStatus({signedIn: true});
    flush();
    assertTrue(passwordsSection.$.manageLink.hidden);
  });

  test('showLinkToPasswordManagerWhenNotEncrypted', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    const syncPrefs = getSyncAllPrefs();
    syncPrefs.encryptAllData = false;
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    flush();
    assertFalse(passwordsSection.$.manageLink.hidden);
  });

  test('showLinkToPasswordManagerWhenNotSignedIn', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    const syncPrefs = getSyncAllPrefs();
    simulateSyncStatus({signedIn: false});
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    flush();
    assertFalse(passwordsSection.$.manageLink.hidden);
  });

  test(
      'showPasswordCheckBannerWhenNotCheckedBeforeAndSignedInAndHavePasswords',
      function() {
        // Suppose no check done initially, non-empty list of passwords,
        // signed in.
        assertEquals(
            passwordManager.data.checkStatus.elapsedTimeSinceLastCheck,
            undefined);
        const passwordList = [
          createPasswordEntry({url: 'site1.com', username: 'luigi'}),
        ];
        const passwordsSection = elementFactory.createPasswordsSection(
            passwordManager, passwordList, []);
        return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
          flush();
          assertFalse(
              passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
          assertFalse(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
          assertTrue(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
        });
      });

  test(
      'showPasswordCheckBannerWhenCanceledCheckedBeforeAndSignedInAndHavePasswords',
      async function() {
        // Suppose initial check was canceled, non-empty list of passwords,
        // signed in.
        assertEquals(
            passwordManager.data.checkStatus.elapsedTimeSinceLastCheck,
            undefined);
        const passwordList = [
          createPasswordEntry({url: 'site1.com', username: 'luigi', id: 0}),
          createPasswordEntry({url: 'site2.com', username: 'luigi', id: 1}),
        ];
        passwordManager.data.checkStatus.state = PasswordCheckState.CANCELED;
        passwordManager.data.leakedCredentials = [
          makeCompromisedCredential('site1.com', 'luigi', 'LEAKED'),
        ];
        pluralString.text = '1 compromised password';

        const passwordsSection = elementFactory.createPasswordsSection(
            passwordManager, passwordList, []);

        await passwordManager.whenCalled('getCompromisedCredentials');
        await pluralString.whenCalled('getPluralString');

        flush();
        assertTrue(
            passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
        assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
        assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
        assertEquals(
            pluralString.text,
            passwordsSection.$$('#checkPasswordLeakCount').innerText.trim());
      });

  test('showPasswordCheckLinkButtonWithoutWarningWhenNotSignedIn', function() {
    // Suppose no check done initially, non-empty list of passwords,
    // signed out.
    assertEquals(
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck, undefined);
    const passwordList = [
      createPasswordEntry({url: 'site1.com', username: 'luigi'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    simulateSyncStatus({signedIn: false});
    return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
      flush();
      assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
    });
  });

  test('showPasswordCheckLinkButtonWithoutWarningWhenNoPasswords', function() {
    // Suppose no check done initially, empty list of passwords, signed
    // in.
    assertEquals(
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck, undefined);
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
      flush();
      assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
    });
  });

  test(
      'showPasswordCheckLinkButtonWithoutWarningWhenNoCredentialsLeaked',
      function() {
        // Suppose no leaks initially, non-empty list of passwords, signed in.
        passwordManager.data.leakedCredentials = [];
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck =
            '5 min ago';
        const passwordList = [
          createPasswordEntry({url: 'site1.com', username: 'luigi'}),
        ];
        const passwordsSection = elementFactory.createPasswordsSection(
            passwordManager, passwordList, []);
        return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
          flush();
          assertTrue(
              passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
          assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
          assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
          assertFalse(
              passwordsSection.$$('#checkPasswordLeakDescription').hidden);
          assertTrue(passwordsSection.$$('#checkPasswordWarningIcon').hidden);
          assertTrue(passwordsSection.$$('#checkPasswordLeakCount').hidden);
        });
      });

  test(
      'showPasswordCheckLinkButtonWithWarningWhenSomeCredentialsLeaked',
      function() {
        // Suppose no leaks initially, non-empty list of passwords, signed in.
        passwordManager.data.leakedCredentials = [
          makeCompromisedCredential('one.com', 'test4', 'LEAKED'),
          makeCompromisedCredential('two.com', 'test3', 'PHISHED'),
        ];
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck =
            '5 min ago';
        const passwordList = [
          createPasswordEntry({url: 'site1.com', username: 'luigi'}),
        ];
        const passwordsSection = elementFactory.createPasswordsSection(
            passwordManager, passwordList, []);
        return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
          flush();
          assertTrue(
              passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
          assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
          assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
          assertTrue(
              passwordsSection.$$('#checkPasswordLeakDescription').hidden);
          assertFalse(passwordsSection.$$('#checkPasswordWarningIcon').hidden);
          assertFalse(passwordsSection.$$('#checkPasswordLeakCount').hidden);
        });
      });

  test('makeWarningAppearWhenLeaksDetected', function() {
    // Suppose no leaks detected initially, non-empty list of passwords,
    // signed in.
    assertEquals(
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck, undefined);
    passwordManager.data.leakedCredentials = [];
    passwordManager.data.checkStatus.elapsedTimeSinceLastCheck = '5 min ago';
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'test4', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'test3', id: 1}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
      flush();
      assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordLeakDescription').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordWarningIcon').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordLeakCount').hidden);
      // Suppose two newly detected leaks come in.
      const leakedCredentials = [
        makeCompromisedCredential('one.com', 'test4', 'LEAKED'),
        makeCompromisedCredential('two.com', 'test3', 'PHISHED'),
      ];
      const elapsedTimeSinceLastCheck = 'just now';
      passwordManager.data.leakedCredentials = leakedCredentials;
      passwordManager.data.checkStatus.elapsedTimeSinceLastCheck =
          elapsedTimeSinceLastCheck;
      passwordManager.lastCallback.addCompromisedCredentialsListener(
          leakedCredentials);
      passwordManager.lastCallback.addPasswordCheckStatusListener(
          makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 2,
              /*remaining=*/ 0,
              /*elapsedTime=*/ elapsedTimeSinceLastCheck));
      flush();
      assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordLeakDescription').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordWarningIcon').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordLeakCount').hidden);
    });
  });

  test('makeBannerDisappearWhenSignedOut', function() {
    // Suppose no leaks detected initially, non-empty list of passwords,
    // signed in.
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'test4', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'test3', id: 1}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
      flush();
      assertFalse(passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
      simulateSyncStatus({signedIn: false});
      flush();
      assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
      assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
      assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
    });
  });

  test('clickingCheckPasswordsButtonStartsCheck', async function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    passwordsSection.$$('#checkPasswordsButton').click();
    const router = Router.getInstance();
    assertEquals(routes.CHECK_PASSWORDS, router.currentRoute);
    assertEquals('true', router.getQueryParameters().get('start'));
    const referrer =
        await passwordManager.whenCalled('recordPasswordCheckReferrer');
    assertEquals(
        PasswordManagerProxy.PasswordCheckReferrer.PASSWORD_SETTINGS, referrer);
  });

  test('clickingCheckPasswordsRowStartsCheck', async function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    passwordsSection.$$('#checkPasswordsLinkRow').click();
    const router = Router.getInstance();
    assertEquals(routes.CHECK_PASSWORDS, router.currentRoute);
    assertEquals('true', router.getQueryParameters().get('start'));
    const referrer =
        await passwordManager.whenCalled('recordPasswordCheckReferrer');
    assertEquals(
        PasswordManagerProxy.PasswordCheckReferrer.PASSWORD_SETTINGS, referrer);
  });
});
