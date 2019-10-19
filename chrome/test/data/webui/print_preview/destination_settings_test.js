// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_settings_test', function() {
  /** @enum {string} */
  const TestNames = {
    ChangeDropdownState: 'change dropdown state',
    NoRecentDestinations: 'no recent destinations',
    RecentDestinations: 'recent destinations',
    SaveAsPdfRecent: 'save as pdf recent',
    GoogleDriveRecent: 'google drive recent',
    SelectSaveAsPdf: 'select save as pdf',
    SelectGoogleDrive: 'select google drive',
    SelectRecentDestination: 'select recent destination',
    OpenDialog: 'open dialog',
    TwoAccountsRecentDestinations: 'two accounts recent destinations',
    UpdateRecentDestinations: 'update recent destinations',
    ResetDestinationOnSignOut: 'reset destination on sign out',
  };

  const suiteName = 'DestinationSettingsTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationSettingsElement} */
    let destinationSettings = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {?print_preview.CloudPrintInterface} */
    let cloudPrintInterface = null;

    /** @type {!Array<!print_preview.RecentDestination>} */
    let recentDestinations = [];

    /** @type {!Array<!print_preview.Destination>} */
    let destinations = [];

    /** @type {!Array<string>} */
    let initialAccounts = [];

    /** @type {string} */
    const defaultUser = 'foo@chromium.org';

    /** @override */
    suiteSetup(function() {
      print_preview_test_utils.setupTestListenerElement();
    });

    /** @override */
    setup(function() {
      PolymerTest.clearBody();

      // Stub out native layer and cloud print interface.
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      let localDestinations = [];
      destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
      nativeLayer.setLocalDestinations(localDestinations);
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();

      const model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      destinationSettings =
          document.createElement('print-preview-destination-settings');
      destinationSettings.settings = model.settings;
      destinationSettings.state = print_preview.State.NOT_READY;
      destinationSettings.disabled = true;
      test_util.fakeDataBind(model, destinationSettings, 'settings');
      document.body.appendChild(destinationSettings);
    });

    // Tests that the dropdown is enabled or disabled correctly based on
    // the state.
    test(assert(TestNames.ChangeDropdownState), function() {
      const dropdown = destinationSettings.$.destinationSelect;
      // Initial state: No destination store means that there is no destination
      // yet, so the dropdown is hidden.
      assertTrue(dropdown.hidden);
      destinationSettings.cloudPrintInterface = cloudPrintInterface;

      // Set up the destination store, but no destination yet. Dropdown is still
      // hidden.
      destinationSettings.init(
          'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          [] /* userAccounts */, true /* syncAvailable */);
      assertTrue(dropdown.hidden);

      return test_util
          .eventToPromise(
              print_preview.DestinationStore.EventType
                  .SELECTED_DESTINATION_CAPABILITIES_READY,
              destinationSettings.destinationStore_)
          .then(() => {
            // The capabilities ready event results in |destinationState|
            // changing to SELECTED, which enables and shows the dropdown even
            // though |state| has not yet transitioned to READY. This is to
            // prevent brief losses of focus when the destination changes.
            assertFalse(dropdown.disabled);
            assertFalse(dropdown.hidden);
            destinationSettings.state = print_preview.State.READY;
            destinationSettings.disabled = false;

            // Simulate setting a setting to an invalid value. Dropdown is
            // disabled due to validation error on another control.
            destinationSettings.state = print_preview.State.ERROR;
            destinationSettings.disabled = true;
            assertTrue(dropdown.disabled);

            // Simulate the user fixing the validation error, and then selecting
            // an invalid printer. Dropdown is enabled, so that the user can fix
            // the error.
            destinationSettings.state = print_preview.State.READY;
            destinationSettings.disabled = false;
            destinationSettings.destinationStore_.dispatchEvent(new CustomEvent(
                print_preview.DestinationStore.EventType.ERROR,
                {detail: print_preview.DestinationErrorType.INVALID}));
            Polymer.dom.flush();

            assertEquals(
                print_preview.DestinationState.ERROR,
                destinationSettings.destinationState);
            assertEquals(
                print_preview.Error.INVALID_PRINTER, destinationSettings.error);
            destinationSettings.state = print_preview.State.ERROR;
            destinationSettings.disabled = true;
            assertFalse(dropdown.disabled);

            if (cr.isChromeOS) {
              // Simulate the user having no printers.
              destinationSettings.destinationStore_.dispatchEvent(
                  new CustomEvent(
                      print_preview.DestinationStore.EventType.ERROR, {
                        detail:
                            print_preview.DestinationErrorType.NO_DESTINATIONS
                      }));
              Polymer.dom.flush();

              assertEquals(
                  print_preview.DestinationState.ERROR,
                  destinationSettings.destinationState);
              assertEquals(
                  print_preview.Error.NO_DESTINATIONS,
                  destinationSettings.error);
              destinationSettings.state = print_preview.State.FATAL_ERROR;
              destinationSettings.disabled = true;
              assertTrue(dropdown.disabled);
            }
          });
    });

    /** @return {!print_preview.DestinationOrigin} */
    function getLocalOrigin() {
      return cr.isChromeOS ? print_preview.DestinationOrigin.CROS :
                             print_preview.DestinationOrigin.LOCAL;
    }

    /**
     * Initializes the destination store and destination settings using
     * |destinations| and |recentDestinations|.
     */
    function initialize() {
      // Initialize destination settings.
      destinationSettings.cloudPrintInterface = cloudPrintInterface;
      destinationSettings.setSetting('recentDestinations', recentDestinations);
      destinationSettings.appKioskMode = false;
      destinationSettings.init(
          '' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          initialAccounts, true /* syncAvailable */);
      destinationSettings.state = print_preview.State.READY;
      destinationSettings.disabled = false;
    }

    /** Simulates a user signing in to Chrome. */
    function signIn() {
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(defaultUser));
      cr.webUIListenerCallback('user-accounts-updated', [defaultUser]);
      Polymer.dom.flush();
    }

    /**
     * @param {string} id The id of the local destination.
     * @return {string} The key corresponding to the local destination, with the
     *     origin set correctly based on the platform.
     */
    function makeLocalDestinationKey(id) {
      return id + '/' + getLocalOrigin() + '/';
    }

    /**
     * @param {!Array<string>} expectedDestinations An array of the expected
     *     destinations in the dropdown.
     */
    function assertDropdownItems(expectedDestinations) {
      let options =
          destinationSettings.$.destinationSelect.shadowRoot.querySelectorAll(
              'option:not([hidden])');
      assertEquals(expectedDestinations.length + 1, options.length);
      expectedDestinations.forEach((expectedValue, index) => {
        assertEquals(expectedValue, options[index].value);
      });
      assertEquals('seeMore', options[expectedDestinations.length].value);
    }

    // Tests that the dropdown contains the appropriate destinations when there
    // are no recent destinations.
    test(assert(TestNames.NoRecentDestinations), function() {
      initialize();
      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the Save as PDF
            // destination.
            assertEquals(
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
                destinationSettings.destination.id);
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            assertDropdownItems(['Save as PDF/local/']);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            return test_util.waitBeforeNextRender(destinationSettings);
          })
          .then(() => {
            assertDropdownItems([
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
          });
    });

    // Tests that the dropdown contains the appropriate destinations when there
    // are 3 recent destinations.
    test(assert(TestNames.RecentDestinations), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));

      initialize();

      // Wait for the destinations to be inserted into the store.
      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            assertEquals('ID1', destinationSettings.destination.id);
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
          });
    });

    // Tests that the dropdown contains the appropriate destinations when Save
    // as PDF is one of the recent destinations.
    test(assert(TestNames.SaveAsPdfRecent), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      recentDestinations.splice(
          1, 1,
          print_preview.makeRecentDestination(
              print_preview_test_utils.getSaveAsPdfDestination()));
      initialize();

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            assertEquals('ID1', destinationSettings.destination.id);
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
          });
    });

    // Tests that the dropdown contains the appropriate destinations when
    // Google Drive is in the recent destinations.
    test(assert(TestNames.GoogleDriveRecent), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      recentDestinations.splice(
          1, 1,
          print_preview.makeRecentDestination(
              print_preview_test_utils.getGoogleDriveDestination(defaultUser)));
      initialize();

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            assertEquals('ID1', destinationSettings.destination.id);
            assertFalse(destinationSettings.$.destinationSelect.disabled);

            // Google Drive does not show up even though it is recent, since the
            // user is not signed in and the destination is not available.
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);
          });
    });

    // Tests that selecting the Save as PDF destination results in the
    // DESTINATION_SELECT event firing, with Save as PDF set as the current
    // destination.
    test(assert(TestNames.SelectSaveAsPdf), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      recentDestinations.splice(
          1, 1,
          print_preview.makeRecentDestination(
              print_preview_test_utils.getSaveAsPdfDestination()));
      initialize();

      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            assertEquals('ID1', destinationSettings.destination.id);
            assertFalse(dropdown.disabled);
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);
            // Most recent destination is selected by default.
            assertEquals('ID1', destinationSettings.destination.id);

            // Simulate selection of Save as PDF printer.
            const whenDestinationSelect = test_util.eventToPromise(
                print_preview.DestinationStore.EventType.DESTINATION_SELECT,
                destinationSettings.destinationStore_);
            dropdown.fire('selected-option-change', 'Save as PDF/local/');

            // Ensure this fires the destination select event.
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals(
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
                destinationSettings.destination.id);
          });
    });

    // Tests that selecting the Google Drive destination results in the
    // DESTINATION_SELECT event firing, with Google Drive set as the current
    // destination.
    test(assert(TestNames.SelectGoogleDrive), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      recentDestinations.splice(
          1, 1,
          print_preview.makeRecentDestination(
              print_preview_test_utils.getGoogleDriveDestination(defaultUser)));
      initialize();
      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            assertEquals('ID1', destinationSettings.destination.id);
            assertFalse(dropdown.disabled);

            // If the user is signed in, Save to Drive should be displayed.
            signIn();
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);

            // Most recent destination is still selected.
            assertEquals('ID1', destinationSettings.destination.id);

            // Simulate selection of Google Drive printer.
            const whenDestinationSelect = test_util.eventToPromise(
                print_preview.DestinationStore.EventType.DESTINATION_SELECT,
                destinationSettings.destinationStore_);
            dropdown.fire(
                'selected-option-change',
                '__google__docs/cookies/foo@chromium.org');
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals(
                print_preview.Destination.GooglePromotedId.DOCS,
                destinationSettings.destination.id);
          });
    });

    // Tests that selecting a recent destination results in the
    // DESTINATION_SELECT event firing, with the recent destination set as the
    // current destination.
    test(assert(TestNames.SelectRecentDestination), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      initialize();
      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            assertEquals('ID1', destinationSettings.destination.id);
            assertFalse(dropdown.disabled);
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            // Simulate selection of Save as PDF printer.
            const whenDestinationSelect = test_util.eventToPromise(
                print_preview.DestinationStore.EventType.DESTINATION_SELECT,
                destinationSettings.destinationStore_);
            dropdown.fire(
                'selected-option-change', makeLocalDestinationKey('ID2'));
            return whenDestinationSelect;
          })
          .then(() => {
            assertEquals('ID2', destinationSettings.destination.id);
          });
    });

    // Tests that selecting the 'see more' option opens the dialog.
    test(assert(TestNames.OpenDialog), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      initialize();
      const dropdown = destinationSettings.$.destinationSelect;

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            assertEquals('ID1', destinationSettings.destination.id);
            assertFalse(dropdown.disabled);
            assertDropdownItems([
              makeLocalDestinationKey('ID1'),
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            dropdown.fire('selected-option-change', 'seeMore');
            return test_util.waitBeforeNextRender(destinationSettings);
          })
          .then(() => {
            assertTrue(
                destinationSettings.$$('print-preview-destination-dialog')
                    .isOpen());
          });
    });

    test(assert(TestNames.TwoAccountsRecentDestinations), function() {
      const account2 = 'bar@chromium.org';
      const driveUser1 =
          print_preview_test_utils.getGoogleDriveDestination(defaultUser);
      const driveUser2 =
          print_preview_test_utils.getGoogleDriveDestination(account2);
      const cloudPrinterUser1 = new print_preview.Destination(
          'FooCloud', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooCloudName',
          print_preview.DestinationConnectionStatus.ONLINE,
          {account: defaultUser});
      const cloudPrinterUser2 = new print_preview.Destination(
          'BarCloud', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'BarCloudName',
          print_preview.DestinationConnectionStatus.ONLINE,
          {account: account2});
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(defaultUser));
      cloudPrintInterface.setPrinter(driveUser2);
      cloudPrintInterface.setPrinter(cloudPrinterUser1);
      cloudPrintInterface.setPrinter(cloudPrinterUser2);

      recentDestinations = [
        cloudPrinterUser1, cloudPrinterUser2, destinations[0]
      ].map(destination => print_preview.makeRecentDestination(destination));

      initialAccounts = [defaultUser, account2];
      initialize();
      Polymer.dom.flush();

      const dropdown = destinationSettings.$.destinationSelect;

      return cloudPrintInterface.whenCalled('printer')
          .then(() => {
            // This will result in the destination store setting the most recent
            // destination.
            assertEquals('FooCloud', destinationSettings.destination.id);
            assertFalse(dropdown.disabled);
            assertDropdownItems([
              'FooCloud/cookies/foo@chromium.org',
              makeLocalDestinationKey('ID1'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);

            dropdown.fire('selected-option-change', 'seeMore');
            return test_util.waitBeforeNextRender(destinationSettings);
          })
          .then(() => {
            const dialog =
                destinationSettings.$$('print-preview-destination-dialog');
            assertTrue(dialog.isOpen());
            const whenAdded = test_util.eventToPromise(
                print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
                destinationSettings.destinationStore_);
            // Simulate setting a new account.
            dialog.fire('account-change', account2);
            Polymer.dom.flush();
            return whenAdded;
          })
          .then(() => {
            assertDropdownItems([
              'BarCloud/cookies/bar@chromium.org',
              makeLocalDestinationKey('ID1'),
              'Save as PDF/local/',
              '__google__docs/cookies/bar@chromium.org',
            ]);
          });
    });

    /**
     * @param {!Array<string>} expectedDestinationIds An array of the expected
     *     recent destination ids.
     */
    function assertRecentDestinations(expectedDestinationIds) {
      const recentDestinations =
          destinationSettings.getSettingValue('recentDestinations');
      assertEquals(expectedDestinationIds.length, recentDestinations.length);
      expectedDestinationIds.forEach((expectedId, index) => {
        assertEquals(expectedId, recentDestinations[index].id);
      });
    }

    function selectDestination(destination) {
      destinationSettings.destinationStore_.selectDestination(destination);
      Polymer.dom.flush();
    }

    /**
     * Tests that the destination being set correctly updates the recent
     * destinations array.
     */
    test(assert(TestNames.UpdateRecentDestinations), function() {
      // Recent destinations start out empty.
      assertRecentDestinations([]);
      assertEquals(0, nativeLayer.getCallCount('getPrinterCapabilities'));

      initialize();

      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(() => {
            assertRecentDestinations(['Save as PDF']);
            assertEquals(1, nativeLayer.getCallCount('getPrinterCapabilities'));

            // Simulate setting a destination.
            nativeLayer.resetResolver('getPrinterCapabilities');
            selectDestination(destinations[0]);
            return nativeLayer.whenCalled('getPrinterCapabilities');
          })
          .then(() => {
            assertRecentDestinations(['ID1', 'Save as PDF']);
            assertEquals(1, nativeLayer.getCallCount('getPrinterCapabilities'));

            // Reselect a recent destination. Still 2 destinations, but in a
            // different order.
            nativeLayer.resetResolver('getPrinterCapabilities');
            destinationSettings.$.destinationSelect.dispatchEvent(
                new CustomEvent('selected-option-change', {
                  detail: 'Save as PDF/local/',
                }));
            Polymer.dom.flush();
            assertRecentDestinations(['Save as PDF', 'ID1']);
            // No additional capabilities call, since the destination was
            // previously selected.
            assertEquals(0, nativeLayer.getCallCount('getPrinterCapabilities'));

            // Select a third destination
            selectDestination(destinations[1]);
            return nativeLayer.whenCalled('getPrinterCapabilities');
          })
          .then(() => {
            assertRecentDestinations(['ID2', 'Save as PDF', 'ID1']);
            assertEquals(1, nativeLayer.getCallCount('getPrinterCapabilities'));

            // Select a fourth destination. List does not grow.
            nativeLayer.resetResolver('getPrinterCapabilities');
            selectDestination(destinations[2]);
            return nativeLayer.whenCalled('getPrinterCapabilities');
          })
          .then(() => {
            assertRecentDestinations(['ID3', 'ID2', 'Save as PDF']);
            assertEquals(1, nativeLayer.getCallCount('getPrinterCapabilities'));
          });
    });

    // Tests that the dropdown resets the destination if the user signs out of
    // the account associated with the curret one.
    test(assert(TestNames.ResetDestinationOnSignOut), function() {
      recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));
      const driveDestination =
          print_preview_test_utils.getGoogleDriveDestination(defaultUser);
      recentDestinations.splice(
          0, 1, print_preview.makeRecentDestination(driveDestination));
      cloudPrintInterface.setPrinter(
          print_preview_test_utils.getGoogleDriveDestination(defaultUser));
      initialAccounts = [defaultUser];
      initialize();

      return cloudPrintInterface.whenCalled('printer')
          .then(() => {
            assertEquals(
                print_preview.Destination.GooglePromotedId.DOCS,
                destinationSettings.destination.id);
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            assertDropdownItems([
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
              '__google__docs/cookies/foo@chromium.org',
            ]);

            // Sign out.
            cr.webUIListenerCallback('user-accounts-updated', []);
            Polymer.dom.flush();

            return nativeLayer.whenCalled('getPrinterCapabilities');
          })
          .then(() => {
            assertEquals('ID2', destinationSettings.destination.id);
            assertFalse(destinationSettings.$.destinationSelect.disabled);
            assertDropdownItems([
              makeLocalDestinationKey('ID2'),
              makeLocalDestinationKey('ID3'),
              'Save as PDF/local/',
            ]);

            // Now that the selected destination is local, signing in and out
            // shouldn't impact it.
            signIn();
            assertEquals('ID2', destinationSettings.destination.id);

            cr.webUIListenerCallback('user-accounts-updated', []);
            Polymer.dom.flush();
            assertEquals('ID2', destinationSettings.destination.id);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
