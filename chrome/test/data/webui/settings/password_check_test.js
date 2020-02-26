// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Check Password tests. */

cr.define('settings_passwords_check', function() {
  function createCheckPasswordSection() {
    // Create a passwords-section to use for testing.
    const passwordsSection =
        this.document.createElement('settings-password-check');
    this.document.body.appendChild(passwordsSection);
    Polymer.dom.flush();
    return passwordsSection;
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
      const checkSection = createCheckPasswordSection();
      checkSection.$.controlPasswordCheckButton.click();
      return passwordManager.whenCalled('startBulkPasswordCheck');
    });
  });
});
