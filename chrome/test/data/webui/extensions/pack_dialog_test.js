// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-pack-dialog. */
cr.define('extension_pack_dialog_tests', function() {
  /** @enum {string} */
  const TestNames = {
    Interaction: 'Interaction',
    PackSuccess: 'PackSuccess',
    PackWarning: 'PackWarning',
    PackError: 'PackError',
  };

  /**
   * @implements {extensions.PackDialogDelegate}
   * @constructor
   */
  function MockDelegate() {
    this.mockResponse = null;
    this.rootPromise;
    this.keyPromise;
  }

  MockDelegate.prototype = {
    /** @override */
    choosePackRootDirectory: function() {
      this.rootPromise = new PromiseResolver();
      return this.rootPromise.promise;
    },

    /** @override */
    choosePrivateKeyPath: function() {
      this.keyPromise = new PromiseResolver();
      return this.keyPromise.promise;
    },

    /** @override */
    packExtension: function(rootPath, keyPath, flag, callback) {
      this.rootPath = rootPath;
      this.keyPath = keyPath;
      this.flag = flag;

      if (callback && this.mockResponse) {
        callback(this.mockResponse);
      }
    },
  };

  const suiteName = 'ExtensionPackDialogTests';

  suite(suiteName, function() {
    /** @type {extensions.PackDialog} */
    let packDialog;

    /** @type {MockDelegate} */
    let mockDelegate;

    setup(function() {
      PolymerTest.clearBody();
      mockDelegate = new MockDelegate();
      packDialog = new extensions.PackDialog();
      packDialog.delegate = mockDelegate;
      document.body.appendChild(packDialog);
    });

    test(assert(TestNames.Interaction), function() {
      const dialogElement = packDialog.$$('cr-dialog').getNative();

      expectTrue(extension_test_util.isElementVisible(dialogElement));
      expectEquals('', packDialog.$$('#root-dir').value);
      packDialog.$$('#root-dir-browse').click();
      expectTrue(!!mockDelegate.rootPromise);
      expectEquals('', packDialog.$$('#root-dir').value);
      const kRootPath = 'this/is/a/path';

      const promises = [];
      promises.push(mockDelegate.rootPromise.promise.then(function() {
        expectEquals(kRootPath, packDialog.$$('#root-dir').value);
        expectEquals(kRootPath, packDialog.packDirectory_);
      }));

      Polymer.dom.flush();
      expectEquals('', packDialog.$$('#key-file').value);
      packDialog.$$('#key-file-browse').click();
      expectTrue(!!mockDelegate.keyPromise);
      expectEquals('', packDialog.$$('#key-file').value);
      const kKeyPath = 'here/is/another/path';

      promises.push(mockDelegate.keyPromise.promise.then(function() {
        expectEquals(kKeyPath, packDialog.$$('#key-file').value);
        expectEquals(kKeyPath, packDialog.keyFile_);
      }));

      mockDelegate.rootPromise.resolve(kRootPath);
      mockDelegate.keyPromise.resolve(kKeyPath);

      return Promise.all(promises).then(function() {
        packDialog.$$('.action-button').click();
        expectEquals(kRootPath, mockDelegate.rootPath);
        expectEquals(kKeyPath, mockDelegate.keyPath);
      });
    });

    test(assert(TestNames.PackSuccess), function() {
      const dialogElement = packDialog.$$('cr-dialog').getNative();
      let packDialogAlert;
      let alertElement;

      expectTrue(extension_test_util.isElementVisible(dialogElement));

      const kRootPath = 'this/is/a/path';
      mockDelegate.mockResponse = {
        status: chrome.developerPrivate.PackStatus.SUCCESS
      };

      packDialog.$$('#root-dir-browse').click();
      mockDelegate.rootPromise.resolve(kRootPath);

      return mockDelegate.rootPromise.promise
          .then(() => {
            expectEquals(kRootPath, packDialog.$$('#root-dir').value);
            packDialog.$$('.action-button').click();

            return test_util.flushTasks();
          })
          .then(() => {
            packDialogAlert = packDialog.$$('extensions-pack-dialog-alert');
            alertElement = packDialogAlert.$.dialog.getNative();
            expectTrue(extension_test_util.isElementVisible(alertElement));
            expectTrue(extension_test_util.isElementVisible(dialogElement));
            expectTrue(!!packDialogAlert.$$('.action-button'));

            const wait = test_util.eventToPromise('close', dialogElement);
            // After 'ok', both dialogs should be closed.
            packDialogAlert.$$('.action-button').click();

            return wait;
          })
          .then(() => {
            expectFalse(extension_test_util.isElementVisible(alertElement));
            expectFalse(extension_test_util.isElementVisible(dialogElement));
          });
    });

    test(assert(TestNames.PackError), function() {
      const dialogElement = packDialog.$$('cr-dialog').getNative();
      let packDialogAlert;
      let alertElement;

      expectTrue(extension_test_util.isElementVisible(dialogElement));

      const kRootPath = 'this/is/a/path';
      mockDelegate.mockResponse = {
        status: chrome.developerPrivate.PackStatus.ERROR
      };

      packDialog.$$('#root-dir-browse').click();
      mockDelegate.rootPromise.resolve(kRootPath);

      return mockDelegate.rootPromise.promise.then(() => {
        expectEquals(kRootPath, packDialog.$$('#root-dir').value);
        packDialog.$$('.action-button').click();
        Polymer.dom.flush();

        // Make sure new alert and the appropriate buttons are visible.
        packDialogAlert = packDialog.$$('extensions-pack-dialog-alert');
        alertElement = packDialogAlert.$.dialog.getNative();
        expectTrue(extension_test_util.isElementVisible(alertElement));
        expectTrue(extension_test_util.isElementVisible(dialogElement));
        expectTrue(!!packDialogAlert.$$('.action-button'));

        // After cancel, original dialog is still open and values unchanged.
        packDialogAlert.$$('.action-button').click();
        Polymer.dom.flush();
        expectFalse(extension_test_util.isElementVisible(alertElement));
        expectTrue(extension_test_util.isElementVisible(dialogElement));
        expectEquals(kRootPath, packDialog.$$('#root-dir').value);
      });
    });

    test(assert(TestNames.PackWarning), function() {
      const dialogElement = packDialog.$$('cr-dialog').getNative();
      let packDialogAlert;
      let alertElement;

      expectTrue(extension_test_util.isElementVisible(dialogElement));

      const kRootPath = 'this/is/a/path';
      mockDelegate.mockResponse = {
        status: chrome.developerPrivate.PackStatus.WARNING,
        item_path: 'item_path',
        pem_path: 'pem_path',
        override_flags: 1,
      };

      packDialog.$$('#root-dir-browse').click();
      mockDelegate.rootPromise.resolve(kRootPath);

      return mockDelegate.rootPromise.promise
          .then(() => {
            expectEquals(kRootPath, packDialog.$$('#root-dir').value);
            packDialog.$$('.action-button').click();
            Polymer.dom.flush();

            // Make sure new alert and the appropriate buttons are visible.
            packDialogAlert = packDialog.$$('extensions-pack-dialog-alert');
            alertElement = packDialogAlert.$.dialog.getNative();
            expectTrue(extension_test_util.isElementVisible(alertElement));
            expectTrue(extension_test_util.isElementVisible(dialogElement));
            expectFalse(packDialogAlert.$$('.cancel-button').hidden);
            expectFalse(packDialogAlert.$$('.action-button').hidden);

            // Make sure "proceed anyway" try to pack extension again.
            packDialogAlert.$$('.action-button').click();

            return test_util.flushTasks();
          })
          .then(() => {
            // Make sure packExtension is called again with the right params.
            expectFalse(extension_test_util.isElementVisible(alertElement));
            expectEquals(
                mockDelegate.flag, mockDelegate.mockResponse.override_flags);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
