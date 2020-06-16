// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('account_manager.account_migration_welcome_tests', () => {
  function registerTests() {
    suite('AccountMigrationWelcomeTests', () => {
      const fakeEmail = 'user@example.com';
      let element = null;
      let testBrowserProxy = null;

      setup(() => {
        testBrowserProxy = new TestAccountManagerBrowserProxy();
        account_manager.AccountManagerBrowserProxyImpl.instance_ =
            testBrowserProxy;
        element = document.createElement('account-migration-welcome');
        document.body.appendChild(element);
        element.userEmail_ = fakeEmail;
        Polymer.dom.flush();
      });

      teardown((done) => {
        element.remove();
        // Allow asynchronous tasks to finish.
        setTimeout(done);
      });

      test('closeDialog is called when user clicks "cancel" button', () => {
        const cancelButton = element.shadowRoot.querySelector('#cancel-button');
        cancelButton.click();
        assertEquals(1, testBrowserProxy.getCallCount('closeDialog'));
      });

      test(
          'reauthenticateAccount is called when user clicks "migrate" button',
          () => {
            const migrateButton =
                element.shadowRoot.querySelector('#migrate-button');
            migrateButton.click();

            assertEquals(
                1, testBrowserProxy.getCallCount('reauthenticateAccount'));
            testBrowserProxy.whenCalled('reauthenticateAccount').then(email => {
              assertEquals(fakeEmail, email);
            });
          });
    });
  }

  return {
    registerTests: registerTests,
  };
});
