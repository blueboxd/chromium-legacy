// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer tests for the PasswordsDeviceSection page. */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MultiStoreExceptionEntry, MultiStorePasswordUiEntry, PasswordManagerImpl, PasswordManagerProxy} from 'chrome://settings/settings.js';
import {createExceptionEntry, createMultiStoreExceptionEntry, createMultiStorePasswordEntry, createPasswordEntry} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';
import {simulateStoredAccounts, simulateSyncStatus} from 'chrome://test/settings/sync_test_util.m.js';
import {TestPasswordManagerProxy} from 'chrome://test/settings/test_password_manager_proxy.js';

/**
 * Sets the fake data and creates the element for testing.
 * @param {!TestPasswordManagerProxy} passwordManager
 * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
 * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList
 * @return {!Object}
 */
function createPasswordsDeviceSection(
    passwordManager, passwordList, exceptionList) {
  passwordManager.data.passwords = passwordList;
  passwordManager.data.exceptions = exceptionList;
  const passwordsDeviceSection =
      document.createElement('passwords-device-section');
  document.body.appendChild(passwordsDeviceSection);
  flush();
  return passwordsDeviceSection;
}

/**
 * @param {!Element} subsection The passwords subsection element that will be
 * checked.
 * @param {!Array<!MultiStorePasswordUiEntry>} expectedPasswords The
 *     expected passwords in this subsection.
 * @private
 */
function validatePasswordsSubsection(subsection, expectedPasswords) {
  assertDeepEquals(expectedPasswords, subsection.items);
  const listItemElements = subsection.querySelectorAll('password-list-item');
  for (let index = 0; index < expectedPasswords.length; ++index) {
    const expectedPassword = expectedPasswords[index];
    const listItemElement = listItemElements[index];
    assertTrue(!!listItemElement);
    assertEquals(
        expectedPassword.urls.shown,
        listItemElement.$.originUrl.textContent.trim());
    assertEquals(expectedPassword.urls.link, listItemElement.$.originUrl.href);
    assertEquals(expectedPassword.username, listItemElement.$.username.value);
  }
}

/**
 * @param {!Element} subsection
 * @param {!Array<!MultiStoreExceptionEntry} expectedExceptions
 */
function validateExceptionsSubsection(subsection, expectedExceptions) {
  assertTrue(!!subsection);
  const listItemElements = subsection.querySelectorAll('.list-item');
  assertEquals(expectedExceptions.length, listItemElements.length);
  for (let index = 0; index < expectedExceptions.length; ++index) {
    const expectedException = expectedExceptions[index];
    const listItemElement = listItemElements[index];
    assertTrue(!!listItemElement);
    assertEquals(
        expectedException.urls.shown,
        listItemElement.querySelector('#exception').innerText);
    assertEquals(
        expectedException.urls.link,
        listItemElement.querySelector('#exception').href);
  }
}

suite('PasswordsDeviceSection', function() {
  /** @type {TestPasswordManagerProxy} */
  let passwordManager = null;

  suiteSetup(function() {
    // Enable feature flag.
    loadTimeData.overrideValues({enableAccountStorage: true});
  });

  setup(function() {
    PolymerTest.clearBody();
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.instance_ = passwordManager;

    // The user only enters this page when they are eligible (signed-in but not
    // syncing) and opted-in to account storage.
    simulateStoredAccounts([{
      fullName: 'john doe',
      givenName: 'john',
      email: 'john@gmail.com',
    }]);
    simulateSyncStatus({signedIn: false});
    passwordManager.setIsOptedInForAccountStorageAndNotify(false);
  });

  // Test verifies that the fallback text is displayed when passwords/exceptions
  // are not present.
  test('verifyPasswordsEmptySubsections', function() {
    const passwordsDeviceSection =
        createPasswordsDeviceSection(passwordManager, [], []);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceOnlyPasswordsLabel')
                    .hidden);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceAndAccountPasswordsLabel')
                    .hidden);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceOnlyExceptionsLabel')
                    .hidden);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceAndAccountExceptionsLabel')
                    .hidden);
  });

  // Test verifies that account passwords are not displayed, whereas
  // device-only and device-and-account ones end up in the correct subsection.
  test('verifyPasswordsFilledSubsections', function() {
    const devicePassword = createPasswordEntry(
        {username: 'device', id: 0, fromAccountStore: false});
    const accountPassword = createPasswordEntry(
        {username: 'account', id: 1, fromAccountStore: true});
    // Create duplicate that gets merged.
    const deviceCopyPassword = createPasswordEntry(
        {username: 'both', frontendId: 42, id: 2, fromAccountStore: false});
    const accountCopyPassword = createPasswordEntry(
        {username: 'both', frontendId: 42, id: 3, fromAccountStore: true});

    // Shuffle entries a little.
    const passwordsDeviceSection = createPasswordsDeviceSection(
        passwordManager,
        [
          devicePassword,
          deviceCopyPassword,
          accountPassword,
          accountCopyPassword,
        ],
        []);

    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, [
          createMultiStorePasswordEntry({username: 'device', deviceId: 0}),
        ]);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList, [
          createMultiStorePasswordEntry(
              {username: 'both', deviceId: 2, accountId: 3}),
        ]);
    assertTrue(passwordsDeviceSection.shadowRoot
                   .querySelector('#noDeviceOnlyPasswordsLabel')
                   .hidden);
    assertTrue(passwordsDeviceSection.shadowRoot
                   .querySelector('#noDeviceAndAccountPasswordsLabel')
                   .hidden);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceOnlyExceptionsLabel')
                    .hidden);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceAndAccountExceptionsLabel')
                    .hidden);
  });

  // Test verifies that removing the device copy of a duplicated password
  // removes it from both lists.
  test('verifyPasswordListRemoveDeviceCopy', function() {
    const passwordList = [
      createPasswordEntry({frontendId: 42, id: 10, fromAccountStore: true}),
      createPasswordEntry({frontendId: 42, id: 20, fromAccountStore: false}),
    ];

    const passwordsDeviceSection =
        createPasswordsDeviceSection(passwordManager, passwordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList,
        [createMultiStorePasswordEntry({accountId: 10, deviceId: 20})]);

    // Remove device copy.
    passwordList.splice(1, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList, []);
  });

  // Test verifies that removing the account copy of a duplicated password
  // moves it to the other subsection.
  test('verifyPasswordListRemoveDeviceCopy', function() {
    const passwordList = [
      createPasswordEntry({frontendId: 42, id: 10, fromAccountStore: true}),
      createPasswordEntry({frontendId: 42, id: 20, fromAccountStore: false}),
    ];

    const passwordsDeviceSection =
        createPasswordsDeviceSection(passwordManager, passwordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList,
        [createMultiStorePasswordEntry({accountId: 10, deviceId: 20})]);

    // Remove account copy.
    passwordList.splice(0, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList,
        [createMultiStorePasswordEntry({deviceId: 20})]);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList, []);
  });

  // Test verifies that account exceptions are not displayed, whereas
  // device-only and device-and-account ones end up in the correct subsection.
  test('verifyExceptionsFilledSubsections', function() {
    const deviceException = createExceptionEntry(
        {url: 'device.com', id: 0, fromAccountStore: false});
    const accountException = createExceptionEntry(
        {url: 'account.com', id: 1, fromAccountStore: true});
    // Create duplicate that gets merged.
    const deviceCopyException = createExceptionEntry(
        {url: 'both.com', frontendId: 42, id: 2, fromAccountStore: false});
    const accountCopyException = createExceptionEntry(
        {url: 'both.com', frontendId: 42, id: 3, fromAccountStore: true});

    // Shuffle entries a little.
    const passwordsDeviceSection =
        createPasswordsDeviceSection(passwordManager, [], [
          deviceException,
          deviceCopyException,
          accountException,
          accountCopyException,
        ]);

    validateExceptionsSubsection(
        passwordsDeviceSection.$.deviceOnlyExceptionsList,
        [createMultiStoreExceptionEntry({url: 'device.com', deviceId: 0})]);
    validateExceptionsSubsection(
        passwordsDeviceSection.$.deviceAndAccountExceptionsList,
        [createMultiStoreExceptionEntry(
            {url: 'both.com', deviceId: 2, accountId: 3})]);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceOnlyPasswordsLabel')
                    .hidden);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceAndAccountPasswordsLabel')
                    .hidden);
    assertTrue(passwordsDeviceSection.shadowRoot
                   .querySelector('#noDeviceOnlyExceptionsLabel')
                   .hidden);
    assertTrue(passwordsDeviceSection.shadowRoot
                   .querySelector('#noDeviceAndAccountExceptionsLabel')
                   .hidden);
  });

});
