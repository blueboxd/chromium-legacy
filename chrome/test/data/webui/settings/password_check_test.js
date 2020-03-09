// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Check Password tests. */

cr.define('settings_passwords_check', function() {
  const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

  function createCheckPasswordSection() {
    // Create a passwords-section to use for testing.
    const passwordsSection =
        this.document.createElement('settings-password-check');
    document.body.appendChild(passwordsSection);
    Polymer.dom.flush();
    return passwordsSection;
  }

  /**
   * Helper method used to create a compromised list item.
   * @param {!chrome.passwordsPrivate.CompromisedCredential} entry
   * @return {!PasswordCheckListItemElement}
   */
  function createLeakedPasswordItem(entry) {
    const leakedPasswordItem =
        this.document.createElement('password-check-list-item');
    leakedPasswordItem.item = entry;
    document.body.appendChild(leakedPasswordItem);
    Polymer.dom.flush();
    return leakedPasswordItem;
  }

  /**
   * Helper method that validates a that elements in the compromised credentials
   * list match the expected data.
   * @param {!Element} listElements The iron-list element that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.CompromisedCredential>}
   * passwordList The expected data.
   * @private
   */
  function validateLeakedPasswordsList(listElements, compromisedCredentials) {
    assertEquals(listElements.items.length, compromisedCredentials.length);
    for (let index = 0; index < compromisedCredentials.length; ++index) {
      // The first child is a template, skip and get the real 'first child'.
      const node = Polymer.dom(listElements).children[index + 1];
      assert(node);
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
    test('testCheckAgainButton', function() {
      const checkPasswordSection = createCheckPasswordSection();
      checkPasswordSection.$.controlPasswordCheckButton.click();
      return passwordManager.whenCalled('startBulkPasswordCheck');
    });

    // Test verifies that if no compromised credentials found than list is not
    // shown TODO(https://crbug.com/1047726): add additional checks after
    // UI is implemented
    test('testNoCompromisedCredentials', function() {
      const checkPasswordSection = createCheckPasswordSection();
      assertTrue(checkPasswordSection.$.passwordCheckBody.hidden);
      assertFalse(checkPasswordSection.$.noCompromisedCredentials.hidden);
      validateLeakedPasswordsList(
          checkPasswordSection.$.leakedPasswordList, []);
    });

    // Test verifies that compromised credentials are displayed in a proper way
    test('testSomeCompromisedCredentials', function() {
      const leakedPasswords = [
        autofill_test_util.makeCompromisedCredentials(
            'one.com', 'test4', 'LEAKED'),
        autofill_test_util.makeCompromisedCredentials(
            'two.com', 'test3', 'PHISHED'),
      ];
      const leakedPasswordsInfo =
          autofill_test_util.makeCompromisedCredentialsInfo(
              leakedPasswords, '5 min ago');
      passwordManager.data.leakedCredentials = leakedPasswordsInfo;
      const checkPasswordSection = createCheckPasswordSection();
      return passwordManager.whenCalled('getCompromisedCredentialsInfo')
          .then(() => {
            Polymer.dom.flush();
            assertFalse(checkPasswordSection.$.passwordCheckBody.hidden);
            assertTrue(checkPasswordSection.$.noCompromisedCredentials.hidden);
            validateLeakedPasswordsList(
                checkPasswordSection.$.leakedPasswordList, leakedPasswords);
          });
    });

    // Test verifies that credentials from mobile app shown correctly
    test('testSomeCompromisedCredentials', function() {
      const password = autofill_test_util.makeCompromisedCredentials(
          'one.com', 'test4', 'LEAKED');
      password.changePasswordUrl = null;

      const checkPasswordSection = createLeakedPasswordItem(password);
      assertEquals(checkPasswordSection.$$('changePasswordUrl'), null);
      assert(checkPasswordSection.$$('#changePasswordInApp'));
    });

    // Verify that the More Actions menu opens when the button is clicked.
    test('testMoreActionsMenu', function() {
      const leakedPasswords = [
        autofill_test_util.makeCompromisedCredentials(
            'google.com', 'jdoerrie', 'LEAKED'),
      ];
      const leakedPasswordsInfo =
          autofill_test_util.makeCompromisedCredentialsInfo(
              leakedPasswords, '5 min ago');
      passwordManager.data.leakedCredentials = leakedPasswordsInfo;
      const checkPasswordSection = createCheckPasswordSection();

      return passwordManager.whenCalled('getCompromisedCredentialsInfo')
          .then(() => {
            Polymer.dom.flush();
            assertFalse(checkPasswordSection.$.passwordCheckBody.hidden);
            const listElements = checkPasswordSection.$.leakedPasswordList;
            const node = Polymer.dom(listElements).children[1];
            const menu = checkPasswordSection.$.moreActionsMenu;

            assertFalse(menu.open);
            node.$.more.click();
            assertTrue(menu.open);
          });
    });

    // A changing status is immediately reflected in title, icon and banner.
    test('testUpdatesNumberOfCheckedPasswordsWhileRunning', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 1,
              /*remaining=*/ 1);
      passwordManager.data.leakedCredentials =
          autofill_test_util.makeCompromisedCredentialsInfo([], 'just now');

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        expectEquals(section.status_.state, PasswordCheckState.RUNNING);

        // Change status from running to IDLE.
        assert(!!passwordManager.lastCallback.addPasswordCheckStatusListener);
        passwordManager.lastCallback.addPasswordCheckStatusListener(
            autofill_test_util.makePasswordCheckStatus(
                /*state=*/ PasswordCheckState.IDLE,
                /*checked=*/ 2,
                /*remaining=*/ 0));

        Polymer.dom.flush();
        expectEquals(section.status_.state, PasswordCheckState.IDLE);
      });
    });

    // Tests that the status is queried right when the page loads.
    test('testQueriesCheckedStatusImmediately', function() {
      const data = passwordManager.data;
      assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
      assertEquals(0, data.leakedCredentials.compromisedCredentials.length);

      const checkPasswordSection = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        expectEquals(
            checkPasswordSection.status_.state, PasswordCheckState.IDLE);
      }, () => assert(false));
    });
  });
});
