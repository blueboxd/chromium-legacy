// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Check Password tests. */

cr.define('settings_passwords_check', function() {
  function createCheckPasswordSection() {
    // Create a passwords-section to use for testing.
    const passwordsSection =
        this.document.createElement('settings-password-check');
    document.body.appendChild(passwordsSection);
    Polymer.dom.flush();
    return passwordsSection;
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
      validateLeakedPasswordsList(
          checkPasswordSection.$.leakedPasswordList, []);
    });

    // Test verifies that compromised credentials are displayed in a proper way
    test('testSomeCompromisedCredentials', function() {
      const leakedPasswords = [
        FakeDataMaker.makeCompromisedCredentials('one.com', 'test4', 'LEAKED'),
        FakeDataMaker.makeCompromisedCredentials('two.com', 'test3', 'PHISHED'),
      ];
      const leakedPasswordsInfo = FakeDataMaker.makeCompromisedCredentialsInfo(
          leakedPasswords, '5 min ago');
      passwordManager.data.leakedCredentials = leakedPasswordsInfo;
      const checkPasswordSection = createCheckPasswordSection();
      return passwordManager.whenCalled('getCompromisedCredentialsInfo')
          .then(() => {
            Polymer.dom.flush();
            assertFalse(checkPasswordSection.$.passwordCheckBody.hidden);
            validateLeakedPasswordsList(
                checkPasswordSection.$.leakedPasswordList, leakedPasswords);
          });
    });
  });
});
