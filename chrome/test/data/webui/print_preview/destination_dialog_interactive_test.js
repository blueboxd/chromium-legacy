// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_dialog_interactive_test', function() {
  /** @enum {string} */
  const TestNames = {
    FocusSearchBox: 'focus search box',
    FocusSearchBoxOnSignIn: 'focus search box on sign in',
    EscapeSearchBox: 'escape search box',
  };

  const suiteName = 'DestinationDialogInteractiveTest';

  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationDialogElement} */
    let dialog = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @override */
    suiteSetup(function() {
      print_preview_test_utils.setupTestListenerElement();
    });

    /** @override */
    setup(function() {
      PolymerTest.clearBody();

      // Create destinations.
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      const localDestinations = [];
      const destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
      const recentDestinations =
          [print_preview.makeRecentDestination(destinations[4])];
      nativeLayer.setLocalDestinations(localDestinations);
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();

      const model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      // Create destination settings, so  that the user manager is created.
      const destinationSettings =
          document.createElement('print-preview-destination-settings');
      destinationSettings.settings = model.settings;
      destinationSettings.state = print_preview.State.READY;
      destinationSettings.disabled = false;
      test_util.fakeDataBind(model, destinationSettings, 'settings');
      document.body.appendChild(destinationSettings);

      // Initialize
      destinationSettings.cloudPrintInterface = cloudPrintInterface;
      destinationSettings.init(
          'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          [] /* userAccounts */, true /* syncAvailable */);
      return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
        // Retrieve a reference to dialog
        dialog = destinationSettings.$.destinationDialog.get();
      });
    });

    // Tests that the search input text field is automatically focused when the
    // dialog is shown.
    test(assert(TestNames.FocusSearchBox), function() {
      const searchInput = dialog.$.searchBox.getSearchInput();
      assertTrue(!!searchInput);
      const whenFocusDone = test_util.eventToPromise('focus', searchInput);
      dialog.destinationStore.startLoadAllDestinations();
      dialog.show();
      return whenFocusDone;
    });

    // Tests that the search input text field is automatically focused when the
    // user signs in successfully after clicking the sign in link. See
    // https://crbug.com/924921
    test(assert(TestNames.FocusSearchBoxOnSignIn), function() {
      const searchInput = dialog.$.searchBox.getSearchInput();
      assertTrue(!!searchInput);
      const signInLink = dialog.$$('.sign-in');
      assertTrue(!!signInLink);
      const whenFocusDone = test_util.eventToPromise('focus', searchInput);
      dialog.destinationStore.startLoadAllDestinations();
      dialog.show();
      return whenFocusDone
          .then(() => {
            signInLink.focus();
            nativeLayer.setSignIn([]);
            signInLink.click();
            return nativeLayer.whenCalled('signIn');
          })
          .then(() => {
            // Link stays focused until successful signin.
            // See https://crbug.com/979603.
            assertEquals(signInLink, dialog.shadowRoot.activeElement);
            nativeLayer.setSignIn(['foo@chromium.org']);
            const whenSearchFocused =
                test_util.eventToPromise('focus', searchInput);
            signInLink.click();
            return whenSearchFocused;
          })
          .then(() => {
            assertEquals('foo@chromium.org', dialog.activeUser);
            assertEquals(1, dialog.users.length);
          });
    });

    // Tests that pressing the escape key while the search box is focused
    // closes the dialog if and only if the query is empty.
    test(assert(TestNames.EscapeSearchBox), function() {
      const searchInput = dialog.$.searchBox.getSearchInput();
      assertTrue(!!searchInput);
      const whenFocusDone = test_util.eventToPromise('focus', searchInput);
      dialog.destinationStore.startLoadAllDestinations();
      dialog.show();
      return whenFocusDone
          .then(() => {
            assertTrue(dialog.$.dialog.open);

            // Put something in the search box.
            const whenSearchChanged =
                test_util.eventToPromise('search-changed', dialog.$.searchBox);
            dialog.$.searchBox.setValue('query');
            return whenSearchChanged;
          })
          .then(() => {
            assertEquals('query', searchInput.value);

            // Simulate escape
            const whenKeyDown = test_util.eventToPromise('keydown', dialog);
            MockInteractions.keyDownOn(searchInput, 19, [], 'Escape');
            return whenKeyDown;
          })
          .then(() => {
            // Dialog should still be open.
            assertTrue(dialog.$.dialog.open);

            // Clear the search box.
            const whenSearchChanged =
                test_util.eventToPromise('search-changed', dialog.$.searchBox);
            dialog.$.searchBox.setValue('');
            return whenSearchChanged;
          })
          .then(() => {
            assertEquals('', searchInput.value);

            // Simulate escape
            const whenKeyDown = test_util.eventToPromise('keydown', dialog);
            MockInteractions.keyDownOn(searchInput, 19, [], 'Escape');
            return whenKeyDown;
          })
          .then(() => {
            // Dialog is closed.
            assertFalse(dialog.$.dialog.open);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
