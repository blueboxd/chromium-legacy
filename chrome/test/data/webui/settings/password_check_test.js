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

  function isElementVisible(element) {
    return !!element && !element.hidden && element.style.display != 'none';
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

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        assert(isElementVisible(section.$.title));
        expectEquals(
            section.i18n('checkPasswordsProgress', 1, 2),
            section.$.title.innerText);

        // Change status from running to IDLE.
        assert(!!passwordManager.lastCallback.addPasswordCheckStatusListener);
        passwordManager.lastCallback.addPasswordCheckStatusListener(
            autofill_test_util.makePasswordCheckStatus(
                /*state=*/ PasswordCheckState.RUNNING,
                /*checked=*/ 2,
                /*remaining=*/ 0));

        Polymer.dom.flush();
        assert(isElementVisible(section.$.title));
        expectEquals(
            section.i18n('checkPasswordsProgress', 2, 2),
            section.$.title.innerText);
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
            PasswordCheckState.IDLE, checkPasswordSection.status_.state);
      }, () => assert(false));
    });

    // Tests that the spinner is replaced with a checkmark on successful runs.
    test('testShowsCheckmarkIconWhenFinishedWithoutLeaks', function() {
      const data = passwordManager.data;
      assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
      assertEquals(0, data.leakedCredentials.compromisedCredentials.length);

      const checkPasswordSection = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const icon = checkPasswordSection.$$('iron-icon');
        const spinner = checkPasswordSection.$$('paper-spinner-lite');
        expectFalse(isElementVisible(spinner));
        assert(isElementVisible(icon));
        expectFalse(icon.classList.contains('has-leaks'));
        expectTrue(icon.classList.contains('no-leaks'));
      });
    });

    // Tests that the spinner is replaced with a triangle if leaks were found.
    test('testShowsTriangleIconWhenFinishedWithLeaks', function() {
      const data = passwordManager.data;
      assertEquals(PasswordCheckState.IDLE, data.checkStatus.state);
      data.leakedCredentials =
          autofill_test_util.makeCompromisedCredentialsInfo(
              [
                autofill_test_util.makeCompromisedCredentials(
                    'one.com', 'test4', 'LEAKED'),
              ],
              'just now');

      const checkPasswordSection = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const icon = checkPasswordSection.$$('iron-icon');
        const spinner = checkPasswordSection.$$('paper-spinner-lite');
        expectFalse(isElementVisible(spinner));
        assert(isElementVisible(icon));
        expectTrue(icon.classList.contains('has-leaks'));
        expectFalse(icon.classList.contains('no-leaks'));
      });
    });

    // Tests that the spinner is replaced with a warning on errors.
    test('testShowsInfoIconWhenFinishedWithErrors', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.OFFLINE,
              /*checked=*/ undefined,
              /*remaining=*/ undefined);

      const checkPasswordSection = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const icon = checkPasswordSection.$$('iron-icon');
        const spinner = checkPasswordSection.$$('paper-spinner-lite');
        expectFalse(isElementVisible(spinner));
        assert(isElementVisible(icon));
        expectFalse(icon.classList.contains('has-leaks'));
        expectFalse(icon.classList.contains('no-leaks'));
      });
    });

    // Tests that the spinner replaces any icon while the check is running.
    test('testShowsSpinnerWhileRunning', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 1,
              /*remaining=*/ 3);

      const checkPasswordSection = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const icon = checkPasswordSection.$$('iron-icon');
        const spinner = checkPasswordSection.$$('paper-spinner-lite');
        expectTrue(isElementVisible(spinner));
        expectFalse(isElementVisible(icon));
      });
    });

    // While running, the check should show the processed and total passwords.
    test('testShowOnlyProgressWhileRunningWithoutLeaks', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 1,
              /*remaining=*/ 3);
      passwordManager.data.leakedCredentials =
          autofill_test_util.makeCompromisedCredentialsInfo([], 'just now');

      section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        assert(isElementVisible(title));
        expectEquals(
            section.i18n('checkPasswordsProgress', 1, 4), title.innerText);
        expectFalse(isElementVisible(section.$.subtitle));
      });
    });

    // While running, show progress and already found leak count.
    test('testShowProgressAndLeaksWhileRunning', function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.RUNNING,
          /*checked=*/ 2,
          /*remaining=*/ 3);
      data.leakedCredentials =
          autofill_test_util.makeCompromisedCredentialsInfo(
              [
                autofill_test_util.makeCompromisedCredentials(
                    'one.com', 'test4', 'LEAKED'),
              ],
              'just now');

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        const subtitle = section.$.subtitle;
        assert(isElementVisible(title));
        assert(isElementVisible(subtitle));
        expectEquals(
            section.i18n('checkPasswordsProgress', 2, 5), title.innerText);
      });
    });

    // When canceled, show string explaining that and already found leak
    // count.
    test('testShowProgressAndLeaksAfterCanceled', function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.CANCELED,
          /*checked=*/ 2,
          /*remaining=*/ 3);
      data.leakedCredentials =
          autofill_test_util.makeCompromisedCredentialsInfo(
              [
                autofill_test_util.makeCompromisedCredentials(
                    'one.com', 'test4', 'LEAKED'),
              ],
              'just now');

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        const subtitle = section.$.subtitle;
        assert(isElementVisible(title));
        assert(isElementVisible(subtitle));
        expectEquals(section.i18n('checkPasswordsCanceled'), title.innerText);
      });
    });

    // After running, show confirmation, timestamp and number of leaks.
    test('testShowLeakCountWhenIdle', function() {
      const data = passwordManager.data;
      data.checkStatus = autofill_test_util.makePasswordCheckStatus(
          /*state=*/ PasswordCheckState.IDLE,
          /*checked=*/ 4,
          /*remaining=*/ 0);
      data.leakedCredentials =
          autofill_test_util.makeCompromisedCredentialsInfo(
              [
                autofill_test_util.makeCompromisedCredentials(
                    'one.com', 'test4', 'LEAKED'),
              ],
              'just now');

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        const subtitle = section.$.subtitle;
        assert(isElementVisible(title));
        assert(isElementVisible(subtitle));
        expectEquals(
            section.i18n('checkPasswords') + ' • just now', title.innerText);
      });
    });

    // When offline, only show an error.
    test('testShowOnlyErrorWhenOffline', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.OFFLINE);

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        assert(isElementVisible(title));
        expectEquals(
            section.i18n('checkPasswordsErrorOffline'), title.innerText);
        expectFalse(isElementVisible(section.$.subtitle));
      });
    });

    // When signed out, only show an error.
    test('testShowOnlyErrorWhenSignedOut', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.SIGNED_OUT);

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        assert(isElementVisible(title));
        expectEquals(
            section.i18n('checkPasswordsErrorSignedOut'), title.innerText);
        expectFalse(isElementVisible(section.$.subtitle));
      });
    });

    // When no passwords are saved, only show an error.
    test('testShowOnlyErrorWithoutPasswords', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.NO_PASSWORDS);

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        assert(isElementVisible(title));
        expectEquals(
            section.i18n('checkPasswordsErrorNoPasswords'), title.innerText);
        expectFalse(isElementVisible(section.$.subtitle));
      });
    });

    // When too many passwords were saved to check them, only show an error.
    test('testShowOnlyErrorWhenTooManyPasswords', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.TOO_MANY_PASSWORDS);

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        assert(isElementVisible(title));
        // TODO(crbug.com/1047726): Check for account redirection.
        expectEquals(
            section.i18n('checkPasswordsErrorTooManyPasswords'),
            title.innerText);
        expectFalse(isElementVisible(section.$.subtitle));
      });
    });

    // When users run out of quota, only show an error.
    test('testShowOnlyErrorWhenQuotaIsHit', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.QUOTA_LIMIT);

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        assert(isElementVisible(title));
        expectEquals(section.i18n('checkPasswordsErrorQuota'), title.innerText);
        expectFalse(isElementVisible(section.$.subtitle));
      });
    });

    // When a general error occurs, only show the message.
    test('testShowOnlyGenericError', function() {
      passwordManager.data.checkStatus =
          autofill_test_util.makePasswordCheckStatus(
              PasswordCheckState.OTHER_ERROR);

      const section = createCheckPasswordSection();
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        const title = section.$.title;
        assert(isElementVisible(title));
        expectEquals(
            section.i18n('checkPasswordsErrorGeneric'), title.innerText);
        expectFalse(isElementVisible(section.$.subtitle));
      });
    });
  });
});
