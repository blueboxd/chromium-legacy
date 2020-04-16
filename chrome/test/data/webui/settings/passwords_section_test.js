// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password Settings tests. */

// clang-format off
// #import {PasswordManagerProxy, PasswordManagerImpl, PluralStringProxyImpl, routes, Router} from 'chrome://settings/settings.js';
// #import {getToastManager} from 'chrome://settings/lazy_load.js';
// #import {PasswordSectionElementFactory, createExceptionEntry, createPasswordEntry, makeCompromisedCredential, makePasswordCheckStatus} from 'chrome://test/settings/passwords_and_autofill_fake_data.m.js';
// #import {runStartExportTest, runExportFlowFastTest, runExportFlowErrorTest, runExportFlowErrorRetryTest, runExportFlowSlowTest, runCancelExportTest, runFireCloseEventAfterExportCompleteTest} from 'chrome://test/settings/passwords_export_test.m.js';
// #import {eventToPromise} from 'chrome://test/test_util.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {TestPasswordManagerProxy} from 'chrome://test/settings/test_password_manager_proxy.m.js';
// #import {TestPluralStringProxy} from 'chrome://test/settings/test_plural_string_proxy.m.js';
// #import {getSyncAllPrefs, simulateSyncStatus} from 'chrome://test/settings/sync_test_util.m.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// clang-format on

cr.define('settings_passwords_section', function() {
  const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

  /**
   * Helper method that validates a that elements in the password list match
   * the expected data.
   * @param {!Element} passwordsSection The passwords section element that will
   *     be checked.
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList The
   *     expected data.
   * @private
   */
  function validatePasswordList(passwordsSection, passwordList) {
    const listElement = passwordsSection.$.passwordList;
    assertEquals(passwordList.length, listElement.items.length);
    for (let index = 0; index < passwordList.length; ++index) {
      const listItems =
          passwordsSection.shadowRoot.querySelectorAll('password-list-item');
      const node = listItems[index];
      assertTrue(!!node);
      const passwordInfo = passwordList[index];
      assertEquals(
          passwordInfo.urls.shown, node.$$('#originUrl').textContent.trim());
      assertEquals(passwordInfo.urls.link, node.$$('#originUrl').href);
      assertEquals(passwordInfo.username, node.$$('#username').value);
      assertDeepEquals(listElement.items[index].entry, passwordInfo);
    }
  }

  /**
   * Helper method that validates a that elements in the exception list match
   * the expected data.
   * @param {!Array<!Element>} nodes The nodes that will be checked.
   * @param {!Array<!chrome.passwordsPrivate.ExceptionEntry>} exceptionList The
   *     expected data.
   * @private
   */
  function validateExceptionList(nodes, exceptionList) {
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
      if (passwordList[i].urls.origin == url) {
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
      if (exceptionList[i].urls.orginUrl == url) {
        return true;
      }
    }
    return false;
  }

  suite('PasswordsSection', function() {
    /** @type {TestPasswordManagerProxy} */
    let passwordManager = null;

    /** @type {autofill_test_util.PasswordSectionElementFactory} */
    let elementFactory = null;

    /** @type {TestPluralStringProxy} */
    let pluaralString = null;

    suiteSetup(function() {
      loadTimeData.overrideValues({enablePasswordCheck: true});
    });

    setup(function() {
      PolymerTest.clearBody();
      // Override the PasswordManagerImpl for testing.
      passwordManager = new TestPasswordManagerProxy();
      pluaralString = new TestPluralStringProxy();
      settings.PluralStringProxyImpl.instance_ = pluaralString;

      PasswordManagerImpl.instance_ = passwordManager;
      elementFactory =
          new autofill_test_util.PasswordSectionElementFactory(document);
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
      Polymer.dom.flush();

      assertTrue(!!element.$$('#passwordsExtensionIndicator'));
    });

    test('verifyNoSavedPasswords', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      validatePasswordList(passwordsSection, []);

      assertFalse(passwordsSection.$.noPasswordsLabel.hidden);
      assertTrue(passwordsSection.$.savedPasswordsHeaders.hidden);
    });

    test('verifySavedPasswordLength', function() {
      const passwordList = [
        autofill_test_util.createPasswordEntry('site1.com', 'luigi'),
        autofill_test_util.createPasswordEntry('longwebsite.com', 'peach'),
        autofill_test_util.createPasswordEntry('site2.com', 'mario'),
        autofill_test_util.createPasswordEntry('site1.com', 'peach'),
        autofill_test_util.createPasswordEntry('google.com', 'mario'),
        autofill_test_util.createPasswordEntry('site2.com', 'luigi'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      // Assert that the data is passed into the iron list. If this fails,
      // then other expectations will also fail.
      assertDeepEquals(
          passwordList,
          passwordsSection.$.passwordList.items.map(entry => entry.entry));

      validatePasswordList(passwordsSection, passwordList);

      assertTrue(passwordsSection.$.noPasswordsLabel.hidden);
      assertFalse(passwordsSection.$.savedPasswordsHeaders.hidden);
    });

    // Test verifies that removing a password will update the elements.
    test('verifyPasswordListRemove', function() {
      const passwordList = [
        autofill_test_util.createPasswordEntry(
            'anotherwebsite.com', 'luigi', 0),
        autofill_test_util.createPasswordEntry('longwebsite.com', 'peach', 1),
        autofill_test_util.createPasswordEntry('website.com', 'mario', 2)
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      validatePasswordList(passwordsSection, passwordList);
      // Simulate 'longwebsite.com' being removed from the list.
      passwordList.splice(1, 1);
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();

      assertFalse(listContainsUrl(
          passwordsSection.savedPasswords.map(entry => entry.entry),
          'longwebsite.com'));
      assertFalse(listContainsUrl(passwordList, 'longwebsite.com'));

      validatePasswordList(passwordsSection, passwordList);
    });

    // Test verifies that adding a password will update the elements.
    test('verifyPasswordListAdd', function() {
      const passwordList = [
        autofill_test_util.createPasswordEntry(
            'anotherwebsite.com', 'luigi', 0),
        autofill_test_util.createPasswordEntry('longwebsite.com', 'peach', 1),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      validatePasswordList(passwordsSection, passwordList);
      // Simulate 'website.com' being added to the list.
      passwordList.unshift(
          autofill_test_util.createPasswordEntry('website.com', 'mario', 2));
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();

      validatePasswordList(passwordsSection, passwordList);
    });

    // Test verifies that removing one out of two passwords for the same website
    // will update the elements.
    test('verifyPasswordListRemoveSameWebsite', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      // Set-up initial list.
      let passwordList = [
        autofill_test_util.createPasswordEntry('website.com', 'mario', 0),
        autofill_test_util.createPasswordEntry('website.com', 'luigi', 1)
      ];

      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();
      validatePasswordList(passwordsSection, passwordList);

      // Simulate '(website.com, mario)' being removed from the list.
      passwordList.shift();
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();
      validatePasswordList(passwordsSection, passwordList);

      // Simulate '(website.com, luigi)' being removed from the list as well.
      passwordList = [];
      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();
      validatePasswordList(passwordsSection, passwordList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any passwords.
    test('verifyPasswordItemRemoveButton', function(done) {
      const passwordList = [
        autofill_test_util.createPasswordEntry('one', 'six'),
        autofill_test_util.createPasswordEntry('two', 'five'),
        autofill_test_util.createPasswordEntry('three', 'four'),
        autofill_test_util.createPasswordEntry('four', 'three'),
        autofill_test_util.createPasswordEntry('five', 'two'),
        autofill_test_util.createPasswordEntry('six', 'one'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);

      const firstNode = getFirstPasswordListItem(passwordsSection);
      assertTrue(!!firstNode);
      const firstPassword = passwordList[0];

      passwordManager.onRemoveSavedPassword = function(id) {
        // Verify that the event matches the expected value.
        assertEquals(firstPassword.id, id);

        // Clean up after self.
        passwordManager.onRemoveSavedPassword = null;

        done();
      };

      // Click the remove button on the first password.
      firstNode.$$('#passwordMenu').click();
      passwordsSection.$.menuRemovePassword.click();
    });

    // Test verifies that 'Copy password' button is hidden for Federated
    // (passwordless) credentials. Does not test Copy button.
    test('verifyCopyAbsentForFederatedPasswordInMenu', function() {
      const passwordList = [
        autofill_test_util.createPasswordEntry('one.com', 'hey'),
      ];
      passwordList[0].federationText = 'with chromium.org';

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      Polymer.dom.flush();

      getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
      assertTrue(passwordsSection.$$('#menuCopyPassword').hidden);
    });

    // Test verifies that 'Copy password' button is not hidden for common
    // credentials. Does not test Copy button.
    test('verifyCopyPresentInMenu', function() {
      const passwordList = [
        autofill_test_util.createPasswordEntry('one.com', 'hey'),
      ];
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      Polymer.dom.flush();

      getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
      assertFalse(passwordsSection.$$('#menuCopyPassword').hidden);
    });

    test('verifyFilterPasswords', function() {
      const passwordList = [
        autofill_test_util.createPasswordEntry('one.com', 'SHOW'),
        autofill_test_util.createPasswordEntry('two.com', 'shower'),
        autofill_test_util.createPasswordEntry('three.com/show', 'four'),
        autofill_test_util.createPasswordEntry('four.com', 'three'),
        autofill_test_util.createPasswordEntry('five.com', 'two'),
        autofill_test_util.createPasswordEntry('six-show.com', 'one'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      passwordsSection.filter = 'SHow';
      Polymer.dom.flush();

      const expectedList = [
        autofill_test_util.createPasswordEntry('one.com', 'SHOW'),
        autofill_test_util.createPasswordEntry('two.com', 'shower'),
        autofill_test_util.createPasswordEntry('three.com/show', 'four'),
        autofill_test_util.createPasswordEntry('six-show.com', 'one'),
      ];

      validatePasswordList(passwordsSection, expectedList);
    });

    test('verifyFilterPasswordsWithRemoval', function() {
      const passwordList = [
        autofill_test_util.createPasswordEntry('one.com', 'SHOW', 0),
        autofill_test_util.createPasswordEntry('two.com', 'shower', 1),
        autofill_test_util.createPasswordEntry('three.com/show', 'four', 2),
        autofill_test_util.createPasswordEntry('four.com', 'three', 3),
        autofill_test_util.createPasswordEntry('five.com', 'two', 4),
        autofill_test_util.createPasswordEntry('six-show.com', 'one', 5),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      passwordsSection.filter = 'SHow';
      Polymer.dom.flush();

      let expectedList = [
        autofill_test_util.createPasswordEntry('one.com', 'SHOW', 0),
        autofill_test_util.createPasswordEntry('two.com', 'shower', 1),
        autofill_test_util.createPasswordEntry('three.com/show', 'four', 2),
        autofill_test_util.createPasswordEntry('six-show.com', 'one', 5),
      ];

      validatePasswordList(passwordsSection, expectedList);

      // Simulate removal of three.com/show
      passwordList.splice(2, 1);

      expectedList = [
        autofill_test_util.createPasswordEntry('one.com', 'SHOW', 0),
        autofill_test_util.createPasswordEntry('two.com', 'shower', 1),
        autofill_test_util.createPasswordEntry('six-show.com', 'one', 5),
      ];

      passwordManager.lastCallback.addSavedPasswordListChangedListener(
          passwordList);
      Polymer.dom.flush();
      validatePasswordList(passwordsSection, expectedList);
    });

    test('verifyFilterPasswordExceptions', function() {
      const exceptionList = [
        autofill_test_util.createExceptionEntry('docsshoW.google.com'),
        autofill_test_util.createExceptionEntry('showmail.com'),
        autofill_test_util.createExceptionEntry('google.com'),
        autofill_test_util.createExceptionEntry('inbox.google.com'),
        autofill_test_util.createExceptionEntry('mapsshow.google.com'),
        autofill_test_util.createExceptionEntry('plus.google.comshow'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [], exceptionList);
      passwordsSection.filter = 'shOW';
      Polymer.dom.flush();

      const expectedExceptionList = [
        autofill_test_util.createExceptionEntry('docsshoW.google.com'),
        autofill_test_util.createExceptionEntry('showmail.com'),
        autofill_test_util.createExceptionEntry('mapsshow.google.com'),
        autofill_test_util.createExceptionEntry('plus.google.comshow'),
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
        autofill_test_util.createExceptionEntry('docs.google.com'),
        autofill_test_util.createExceptionEntry('mail.com'),
        autofill_test_util.createExceptionEntry('google.com'),
        autofill_test_util.createExceptionEntry('inbox.google.com'),
        autofill_test_util.createExceptionEntry('maps.google.com'),
        autofill_test_util.createExceptionEntry('plus.google.com'),
      ];

      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [], exceptionList);

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          exceptionList);

      assertTrue(passwordsSection.$.noExceptionsLabel.hidden);
    });

    // Test verifies that removing an exception will update the elements.
    test('verifyPasswordExceptionRemove', function() {
      const exceptionList = [
        autofill_test_util.createExceptionEntry('docs.google.com'),
        autofill_test_util.createExceptionEntry('mail.com'),
        autofill_test_util.createExceptionEntry('google.com'),
        autofill_test_util.createExceptionEntry('inbox.google.com'),
        autofill_test_util.createExceptionEntry('maps.google.com'),
        autofill_test_util.createExceptionEntry('plus.google.com'),
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
      Polymer.dom.flush();

      validateExceptionList(
          getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
          exceptionList);
    });

    // Test verifies that pressing the 'remove' button will trigger a remove
    // event. Does not actually remove any exceptions.
    test('verifyPasswordExceptionRemoveButton', function(done) {
      const exceptionList = [
        autofill_test_util.createExceptionEntry('docs.google.com'),
        autofill_test_util.createExceptionEntry('mail.com'),
        autofill_test_util.createExceptionEntry('google.com'),
        autofill_test_util.createExceptionEntry('inbox.google.com'),
        autofill_test_util.createExceptionEntry('maps.google.com'),
        autofill_test_util.createExceptionEntry('plus.google.com'),
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

      passwordManager.onRemoveException = function(id) {
        // Verify that the event matches the expected value.
        assertTrue(item < exceptionList.length);
        assertEquals(id, exceptionList[item].id);

        if (++item < exceptionList.length) {
          clickRemoveButton();  // Click 'remove' on all passwords, one by one.
        } else {
          // Clean up after self.
          passwordManager.onRemoveException = null;

          done();
        }
      };

      // Start removing.
      clickRemoveButton();
    });

    test('verifyFederatedPassword', function() {
      const item = autofill_test_util.createPasswordEntry('goo.gl', 'bart');
      item.federationText = 'with chromium.org';
      const passwordDialog = elementFactory.createPasswordEditDialog(item);

      Polymer.dom.flush();

      assertEquals(item.federationText, passwordDialog.$.passwordInput.value);
      // Text should be readable.
      assertEquals('text', passwordDialog.$.passwordInput.type);
      assertTrue(passwordDialog.$.showPasswordButton.hidden);
    });

    test('showSavedPasswordEditDialog', function() {
      const PASSWORD = 'bAn@n@5';
      const item = autofill_test_util.createPasswordEntry('goo.gl', 'bart');
      const passwordDialog = elementFactory.createPasswordEditDialog(item);

      assertFalse(passwordDialog.$.showPasswordButton.hidden);

      passwordDialog.set('item.password', PASSWORD);
      Polymer.dom.flush();

      assertEquals(PASSWORD, passwordDialog.$.passwordInput.value);
      // Password should be visible.
      assertEquals('text', passwordDialog.$.passwordInput.type);
      assertFalse(passwordDialog.$.showPasswordButton.hidden);
    });

    test('showSavedPasswordListItem', function() {
      const PASSWORD = 'bAn@n@5';
      const item = autofill_test_util.createPasswordEntry('goo.gl', 'bart');
      const passwordListItem = elementFactory.createPasswordListItem(item);
      // Hidden passwords should be disabled.
      assertTrue(passwordListItem.$$('#password').disabled);

      passwordListItem.set('item.password', PASSWORD);
      Polymer.dom.flush();

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
      const expectedItem =
          autofill_test_util.createPasswordEntry('goo.gl', 'bart', 1);
      const passwordDialog =
          elementFactory.createPasswordEditDialog(expectedItem);
      assertEquals('', passwordDialog.item.password);

      passwordManager.setPlaintextPassword('password');
      passwordDialog.$.showPasswordButton.click();
      return passwordManager.whenCalled('requestPlaintextPassword')
          .then(({id, reason}) => {
            assertEquals(1, id);
            assertEquals('VIEW', reason);
            assertEquals('password', passwordDialog.item.password);
          });
    });

    test('onShowSavedPasswordListItem', function() {
      const expectedItem =
          autofill_test_util.createPasswordEntry('goo.gl', 'bart', 1);
      const passwordListItem =
          elementFactory.createPasswordListItem(expectedItem);
      assertEquals('', passwordListItem.item.password);

      passwordManager.setPlaintextPassword('password');
      passwordListItem.$$('#showPasswordButton').click();
      return passwordManager.whenCalled('requestPlaintextPassword')
          .then(({id, reason}) => {
            assertEquals(1, id);
            assertEquals('VIEW', reason);
            assertEquals('password', passwordListItem.item.password);
          });
    });

    test('onCopyPasswordListItem', function() {
      const expectedItem =
          autofill_test_util.createPasswordEntry('goo.gl', 'bart', 1);
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [expectedItem], []);

      getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
      passwordsSection.$$('#menuCopyPassword').click();

      return passwordManager.whenCalled('requestPlaintextPassword')
          .then(({id, reason}) => {
            assertEquals(1, id);
            assertEquals('COPY', reason);
          });
    });

    test('closingPasswordsSectionHidesUndoToast', function(done) {
      const passwordEntry =
          autofill_test_util.createPasswordEntry('goo.gl', 'bart');
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [passwordEntry], []);
      const toastManager = cr.toastManager.getToastManager();

      // Click the remove button on the first password and assert that an undo
      // toast is shown.
      getFirstPasswordListItem(passwordsSection).$$('#passwordMenu').click();
      passwordsSection.$.menuRemovePassword.click();
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
        autofill_test_util.createPasswordEntry('googoo.com', 'Larry'),
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
        autofill_test_util.createPasswordEntry('googoo.com', 'Larry'),
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

    if (!cr.isChromeOS) {
      // Test that tapping "Export passwords..." notifies the browser.
      test('startExport', function(done) {
        const exportDialog =
            elementFactory.createExportPasswordsDialog(passwordManager);
        export_passwords_tests.runStartExportTest(
            exportDialog, passwordManager, done);
      });

      // Test the export flow. If exporting is fast, we should skip the
      // in-progress view altogether.
      test('exportFlowFast', function(done) {
        const exportDialog =
            elementFactory.createExportPasswordsDialog(passwordManager);
        export_passwords_tests.runExportFlowFastTest(
            exportDialog, passwordManager, done);
      });

      // The error view is shown when an error occurs.
      test('exportFlowError', function(done) {
        const exportDialog =
            elementFactory.createExportPasswordsDialog(passwordManager);
        export_passwords_tests.runExportFlowErrorTest(
            exportDialog, passwordManager, done);
      });

      // The error view allows to retry.
      test('exportFlowErrorRetry', function(done) {
        const exportDialog =
            elementFactory.createExportPasswordsDialog(passwordManager);
        export_passwords_tests.runExportFlowErrorRetryTest(
            exportDialog, passwordManager, done);
      });

      // Test the export flow. If exporting is slow, Chrome should show the
      // in-progress dialog for at least 1000ms.
      test('exportFlowSlow', function(done) {
        const exportDialog =
            elementFactory.createExportPasswordsDialog(passwordManager);
        export_passwords_tests.runExportFlowSlowTest(
            exportDialog, passwordManager, done);
      });

      // Test that canceling the dialog while exporting will also cancel the
      // export on the browser.
      test('cancelExport', function(done) {
        const exportDialog =
            elementFactory.createExportPasswordsDialog(passwordManager);
        export_passwords_tests.runCancelExportTest(
            exportDialog, passwordManager, done);
      });

      test('fires close event after export complete', () => {
        const exportDialog =
            elementFactory.createExportPasswordsDialog(passwordManager);
        return export_passwords_tests.runFireCloseEventAfterExportCompleteTest(
            exportDialog, passwordManager);
      });
    }

    // The export dialog is dismissable.
    test('exportDismissable', function(done) {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);

      assertTrue(exportDialog.$$('#dialog_start').open);
      exportDialog.$$('#cancelButton').click();
      Polymer.dom.flush();
      assertFalse(!!exportDialog.$$('#dialog_start'));

      done();
    });

    test('fires close event when canceled', () => {
      const exportDialog =
          elementFactory.createExportPasswordsDialog(passwordManager);
      const wait = test_util.eventToPromise(
          'passwords-export-dialog-close', exportDialog);
      exportDialog.$$('#cancelButton').click();
      return wait;
    });

    test('hideLinkToPasswordManagerWhenEncrypted', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();
      syncPrefs.encryptAllData = true;
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      sync_test_util.simulateSyncStatus({signedIn: true});
      Polymer.dom.flush();
      assertTrue(passwordsSection.$.manageLink.hidden);
    });

    test('showLinkToPasswordManagerWhenNotEncrypted', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();
      syncPrefs.encryptAllData = false;
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      Polymer.dom.flush();
      assertFalse(passwordsSection.$.manageLink.hidden);
    });

    test('showLinkToPasswordManagerWhenNotSignedIn', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      const syncPrefs = sync_test_util.getSyncAllPrefs();
      sync_test_util.simulateSyncStatus({signedIn: false});
      cr.webUIListenerCallback('sync-prefs-changed', syncPrefs);
      Polymer.dom.flush();
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
            autofill_test_util.createPasswordEntry('site1.com', 'luigi'),
          ];
          const passwordsSection = elementFactory.createPasswordsSection(
              passwordManager, passwordList, []);
          return passwordManager.whenCalled('getPasswordCheckStatus')
              .then(() => {
                Polymer.dom.flush();
                assertFalse(
                    passwordsSection.$$('#checkPasswordsBannerContainer')
                        .hidden);
                assertFalse(
                    passwordsSection.$$('#checkPasswordsButtonRow').hidden);
                assertTrue(
                    passwordsSection.$$('#checkPasswordsLinkRow').hidden);
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
            autofill_test_util.createPasswordEntry('site1.com', 'luigi'),
            autofill_test_util.createPasswordEntry('site2.com', 'luigi'),
          ];
          passwordManager.data.checkStatus.state = PasswordCheckState.CANCELED;
          passwordManager.data.leakedCredentials = [
            autofill_test_util.makeCompromisedCredential(
                'site1.com', 'luigi', 'LEAKED'),
          ];
          pluaralString.text = '1 compromised password';

          const passwordsSection = elementFactory.createPasswordsSection(
              passwordManager, passwordList, []);

          await passwordManager.whenCalled('getCompromisedCredentials');
          await pluaralString.whenCalled('getPluralString');

          Polymer.dom.flush();
          assertTrue(
              passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
          assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
          assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
          assertEquals(
              pluaralString.text,
              passwordsSection.$$('#checkPasswordLeakCount').innerText.trim());
        });

    test(
        'showPasswordCheckLinkButtonWithoutWarningWhenNotSignedIn', function() {
          // Suppose no check done initially, non-empty list of passwords,
          // signed out.
          assertEquals(
              passwordManager.data.checkStatus.elapsedTimeSinceLastCheck,
              undefined);
          const passwordList = [
            autofill_test_util.createPasswordEntry('site1.com', 'luigi'),
          ];
          const passwordsSection = elementFactory.createPasswordsSection(
              passwordManager, passwordList, []);
          sync_test_util.simulateSyncStatus({signedIn: false});
          return passwordManager.whenCalled('getPasswordCheckStatus')
              .then(() => {
                Polymer.dom.flush();
                assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer')
                               .hidden);
                assertTrue(
                    passwordsSection.$$('#checkPasswordsButtonRow').hidden);
                assertFalse(
                    passwordsSection.$$('#checkPasswordsLinkRow').hidden);
              });
        });

    test(
        'showPasswordCheckLinkButtonWithoutWarningWhenNoPasswords', function() {
          // Suppose no check done initially, empty list of passwords, signed
          // in.
          assertEquals(
              passwordManager.data.checkStatus.elapsedTimeSinceLastCheck,
              undefined);
          const passwordsSection =
              elementFactory.createPasswordsSection(passwordManager, [], []);
          return passwordManager.whenCalled('getPasswordCheckStatus')
              .then(() => {
                Polymer.dom.flush();
                assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer')
                               .hidden);
                assertTrue(
                    passwordsSection.$$('#checkPasswordsButtonRow').hidden);
                assertFalse(
                    passwordsSection.$$('#checkPasswordsLinkRow').hidden);
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
            autofill_test_util.createPasswordEntry('site1.com', 'luigi'),
          ];
          const passwordsSection = elementFactory.createPasswordsSection(
              passwordManager, passwordList, []);
          return passwordManager.whenCalled('getPasswordCheckStatus')
              .then(() => {
                Polymer.dom.flush();
                assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer')
                               .hidden);
                assertTrue(
                    passwordsSection.$$('#checkPasswordsButtonRow').hidden);
                assertFalse(
                    passwordsSection.$$('#checkPasswordsLinkRow').hidden);
                assertFalse(passwordsSection.$$('#checkPasswordLeakDescription')
                                .hidden);
                assertTrue(
                    passwordsSection.$$('#checkPasswordWarningIcon').hidden);
                assertTrue(
                    passwordsSection.$$('#checkPasswordLeakCount').hidden);
              });
        });

    test(
        'showPasswordCheckLinkButtonWithWarningWhenSomeCredentialsLeaked',
        function() {
          // Suppose no leaks initially, non-empty list of passwords, signed in.
          passwordManager.data.leakedCredentials = [
            autofill_test_util.makeCompromisedCredential(
                'one.com', 'test4', 'LEAKED'),
            autofill_test_util.makeCompromisedCredential(
                'two.com', 'test3', 'PHISHED'),
          ];
          passwordManager.data.checkStatus.elapsedTimeSinceLastCheck =
              '5 min ago';
          const passwordList = [
            autofill_test_util.createPasswordEntry('site1.com', 'luigi'),
          ];
          const passwordsSection = elementFactory.createPasswordsSection(
              passwordManager, passwordList, []);
          return passwordManager.whenCalled('getPasswordCheckStatus')
              .then(() => {
                Polymer.dom.flush();
                assertTrue(passwordsSection.$$('#checkPasswordsBannerContainer')
                               .hidden);
                assertTrue(
                    passwordsSection.$$('#checkPasswordsButtonRow').hidden);
                assertFalse(
                    passwordsSection.$$('#checkPasswordsLinkRow').hidden);
                assertTrue(passwordsSection.$$('#checkPasswordLeakDescription')
                               .hidden);
                assertFalse(
                    passwordsSection.$$('#checkPasswordWarningIcon').hidden);
                assertFalse(
                    passwordsSection.$$('#checkPasswordLeakCount').hidden);
              });
        });

    test('makeWarningAppearWhenLeaksDetected', function() {
      // Suppose no leaks detected initially, non-empty list of passwords,
      // signed in.
      assertEquals(
          passwordManager.data.checkStatus.elapsedTimeSinceLastCheck,
          undefined);
      passwordManager.data.leakedCredentials = [];
      passwordManager.data.checkStatus.elapsedTimeSinceLastCheck = '5 min ago';
      const passwordList = [
        autofill_test_util.createPasswordEntry('one.com', 'test4'),
        autofill_test_util.createPasswordEntry('two.com', 'test3'),
      ];
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        assertTrue(
            passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
        assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
        assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
        assertFalse(
            passwordsSection.$$('#checkPasswordLeakDescription').hidden);
        assertTrue(passwordsSection.$$('#checkPasswordWarningIcon').hidden);
        assertTrue(passwordsSection.$$('#checkPasswordLeakCount').hidden);
        // Suppose two newly detected leaks come in.
        const leakedCredentials = [
          autofill_test_util.makeCompromisedCredential(
              'one.com', 'test4', 'LEAKED'),
          autofill_test_util.makeCompromisedCredential(
              'two.com', 'test3', 'PHISHED'),
        ];
        const elapsedTimeSinceLastCheck = 'just now';
        passwordManager.data.leakedCredentials = leakedCredentials;
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck =
            elapsedTimeSinceLastCheck;
        passwordManager.lastCallback.addCompromisedCredentialsListener(
            leakedCredentials);
        passwordManager.lastCallback.addPasswordCheckStatusListener(
            autofill_test_util.makePasswordCheckStatus(
                /*state=*/ PasswordCheckState.RUNNING,
                /*checked=*/ 2,
                /*remaining=*/ 0,
                /*elapsedTime=*/ elapsedTimeSinceLastCheck));
        Polymer.dom.flush();
        assertTrue(
            passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
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
        autofill_test_util.createPasswordEntry('one.com', 'test4'),
        autofill_test_util.createPasswordEntry('two.com', 'test3'),
      ];
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
        Polymer.dom.flush();
        assertFalse(
            passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
        assertFalse(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
        assertTrue(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
        sync_test_util.simulateSyncStatus({signedIn: false});
        Polymer.dom.flush();
        assertTrue(
            passwordsSection.$$('#checkPasswordsBannerContainer').hidden);
        assertTrue(passwordsSection.$$('#checkPasswordsButtonRow').hidden);
        assertFalse(passwordsSection.$$('#checkPasswordsLinkRow').hidden);
      });
    });

    test('clickingCheckPasswordsButtonStartsCheck', async function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      passwordsSection.$$('#checkPasswordsButton').click();
      const router = settings.Router.getInstance();
      assertEquals(settings.routes.CHECK_PASSWORDS, router.currentRoute);
      assertEquals('true', router.getQueryParameters().get('start'));
      const referrer =
          await passwordManager.whenCalled('recordPasswordCheckReferrer');
      assertEquals(
          PasswordManagerProxy.PasswordCheckReferrer.PASSWORD_SETTINGS,
          referrer);
    });

    test('clickingCheckPasswordsRowStartsCheck', async function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      passwordsSection.$$('#checkPasswordsLinkRow').click();
      const router = settings.Router.getInstance();
      assertEquals(settings.routes.CHECK_PASSWORDS, router.currentRoute);
      assertEquals('true', router.getQueryParameters().get('start'));
      const referrer =
          await passwordManager.whenCalled('recordPasswordCheckReferrer');
      assertEquals(
          PasswordManagerProxy.PasswordCheckReferrer.PASSWORD_SETTINGS,
          referrer);
    });
  });
  // #cr_define_end
});
