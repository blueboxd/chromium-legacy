// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_store_test', function() {
  /** @enum {string} */
  const TestNames = {
    SingleRecentDestination: 'single recent destination',
    MultipleRecentDestinations: 'multiple recent destinations',
    MultipleRecentDestinationsOneRequest:
        'multiple recent destinations one request',
    DefaultDestinationSelectionRules: 'default destination selection rules',
    SystemDefaultPrinterPolicy: 'system default printer policy',
    KioskModeSelectsFirstPrinter: 'kiosk mode selects first printer',
    NoPrintersShowsError: 'no printers shows error',
    UnreachableRecentCloudPrinter: 'unreachable recent cloud printer',
    RecentSaveAsPdf: 'recent save as pdf',
    MultipleRecentDestinationsAccounts: 'multiple recent destinations accounts',
  };

  const suiteName = 'DestinationStoreTest';
  suite(suiteName, function() {
    /** @type {?print_preview.DestinationStore} */
    let destinationStore = null;

    /** @type {?print_preview.NativeLayerStub} */
    let nativeLayer = null;

    /** @type {?cloudprint.CloudPrintInterface} */
    let cloudPrintInterface = null;

    /** @type {?print_preview.NativeInitialSettngs} */
    let initialSettings = null;

    /** @type {!Array<!print_preview.LocalDestinationInfo>} */
    let localDestinations = [];

    /** @type {!Array<!print_preview.Destination>} */
    let cloudDestinations = [];

    /** @type {!Array<!print_preview.Destination>} */
    let destinations = [];

    /** @type {number} */
    let numPrintersSelected = 0;

    /** @override */
    setup(function() {
      // Clear the UI.
      PolymerTest.clearBody();

      print_preview_test_utils.setupTestListenerElement();

      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);

      initialSettings = print_preview_test_utils.getDefaultInitialSettings();
      initialSettings.userAccounts = [];
      localDestinations = [];
      destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
    });

    /*
     * Sets the initial settings to the stored value and creates the page.
     * @param {boolean=} opt_expectPrinterFailure Whether printer fetch is
     *     expected to fail
     * @return {!Promise} Promise that resolves when initial settings and,
     *     if printer failure is not expected, printer capabilities have
     *     been returned.
     */
    function setInitialSettings(opt_expectPrinterFailure) {
      // Set local print list.
      nativeLayer.setLocalDestinations(localDestinations);

      // Create cloud print interface.
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();
      cloudDestinations.forEach(cloudDestination => {
        cloudPrintInterface.setPrinter(cloudDestination);
      });

      // Create destination store.
      destinationStore = print_preview_test_utils.createDestinationStore();
      destinationStore.setCloudPrintInterface(cloudPrintInterface);
      destinationStore.addEventListener(
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          function() {
            numPrintersSelected++;
          });
      destinationStore.setActiveUser(
          initialSettings.userAccounts.length > 0 ?
              initialSettings.userAccounts[0] :
              '');

      // Initialize.
      const recentDestinations = initialSettings.serializedAppStateStr ?
          JSON.parse(initialSettings.serializedAppStateStr).recentDestinations :
          [];
      const whenCapabilitiesReady = test_util.eventToPromise(
          print_preview.DestinationStore.EventType
              .SELECTED_DESTINATION_CAPABILITIES_READY,
          destinationStore);
      destinationStore.init(
          initialSettings.isInAppKioskMode, initialSettings.printerName,
          initialSettings.serializedDefaultDestinationSelectionRulesStr,
          recentDestinations);
      return opt_expectPrinterFailure ? Promise.resolve() : Promise.race([
        nativeLayer.whenCalled('getPrinterCapabilities'), whenCapabilitiesReady
      ]);
    }

    /**
     * Tests that if the user has a single valid recent destination the
     * destination is automatically reselected.
     */
    test(assert(TestNames.SingleRecentDestination), function() {
      const recentDestination =
          print_preview.makeRecentDestination(destinations[0]);
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [recentDestination],
      });

      return setInitialSettings().then(function(args) {
        assertEquals('ID1', args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals('ID1', destinationStore.selectedDestination.id);
      });
    });

    /**
     * Tests that if the user has multiple valid recent destinations the most
     * recent destination is automatically reselected and its capabilities are
     * fetched.
     */
    test(assert(TestNames.MultipleRecentDestinations), function() {
      const recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings().then(function(args) {
        // Should have loaded ID1 as the selected printer, since it was most
        // recent.
        assertEquals('ID1', args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals('ID1', destinationStore.selectedDestination.id);
        // Only the most recent printer should have been added to the store.
        const reportedPrinters = destinationStore.destinations();
        destinations.forEach((destination, index) => {
          const match = reportedPrinters.find((reportedPrinter) => {
            return reportedPrinter.id == destination.id;
          });
          assertEquals(index > 0, typeof match === 'undefined');
        });
      });
    });

    /**
     * Tests that if the user has multiple valid recent destinations, this
     * does not result in multiple calls to getPrinterCapabilities and the
     * correct destination is selected for the preview request.
     * For crbug.com/666595.
     */
    test(assert(TestNames.MultipleRecentDestinationsOneRequest), function() {
      const recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings().then(function(args) {
        // Should have loaded ID1 as the selected printer, since it was most
        // recent.
        assertEquals('ID1', args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals('ID1', destinationStore.selectedDestination.id);

        // Most recent printer + Save as PDF are in the store automatically.
        const reportedPrinters = destinationStore.destinations();
        assertEquals(2, reportedPrinters.length);
        destinations.forEach((destination, index) => {
          assertEquals(
              index === 0, reportedPrinters.some(p => p.id == destination.id));
        });
        assertEquals(1, numPrintersSelected);
      });
    });

    /**
     * Tests that if there are default destination selection rules they are
     * respected and a matching destination is automatically selected.
     */
    test(assert(TestNames.DefaultDestinationSelectionRules), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr =
          JSON.stringify({namePattern: '.*Four.*'});
      initialSettings.serializedAppStateStr = '';
      return setInitialSettings().then(function(args) {
        // Should have loaded ID4 as the selected printer, since it matches
        // the rules.
        assertEquals('ID4', args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals('ID4', destinationStore.selectedDestination.id);
      });
    });

    /**
     * Tests that if the system default printer policy is enabled the system
     * default printer is automatically selected even if the user has recent
     * destinations.
     */
    test(assert(TestNames.SystemDefaultPrinterPolicy), function() {
      // Set the policy in loadTimeData.
      loadTimeData.overrideValues({useSystemDefaultPrinter: true});

      // Setup some recent destinations to ensure they are not selected.
      const recentDestinations = [];
      destinations.slice(0, 3).forEach(destination => {
        recentDestinations.push(
            print_preview.makeRecentDestination(destination));
      });

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return Promise
          .all([
            setInitialSettings(),
            test_util.eventToPromise(
                print_preview.DestinationStore.EventType
                    .SELECTED_DESTINATION_CAPABILITIES_READY,
                destinationStore),
          ])
          .then(() => {
            // Need to load FooDevice as the printer, since it is the system
            // default.
            assertEquals('FooDevice', destinationStore.selectedDestination.id);
          });
    });

    /**
     * Tests that if there is no system default destination, the default
     * selection rules and recent destinations are empty, and the preview
     * is in app kiosk mode (so no PDF printer), the first destination returned
     * from printer fetch is selected.
     */
    test(assert(TestNames.KioskModeSelectsFirstPrinter), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
      initialSettings.serializedAppStateStr = '';
      initialSettings.isInAppKioskMode = true;
      initialSettings.printerName = '';

      return setInitialSettings().then(function(args) {
        // Should have loaded the first destination as the selected printer.
        assertEquals(destinations[0].id, args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals(
            destinations[0].id, destinationStore.selectedDestination.id);
      });
    });

    /**
     * Tests that if there is no system default destination, the default
     * selection rules and recent destinations are empty, the preview
     * is in app kiosk mode (so no PDF printer), and there are no
     * destinations found, the NO_DESTINATIONS error is fired and the selected
     * destination is null.
     */
    test(assert(TestNames.NoPrintersShowsError), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
      initialSettings.serializedAppStateStr = '';
      initialSettings.isInAppKioskMode = true;
      initialSettings.printerName = '';
      localDestinations = [];

      return Promise
          .all([
            setInitialSettings(true),
            test_util.eventToPromise(
                print_preview.DestinationStore.EventType.ERROR,
                destinationStore),
          ])
          .then(function(argsArray) {
            const errorEvent = argsArray[1];
            assertEquals(
                print_preview.DestinationErrorType.NO_DESTINATIONS,
                errorEvent.detail);
            assertEquals(null, destinationStore.selectedDestination);
          });
    });

    /**
     * Tests that if the user has a recent destination that triggers a cloud
     * print error this does not disable the dialog.
     */
    test(assert(TestNames.UnreachableRecentCloudPrinter), function() {
      const cloudPrinter =
          print_preview_test_utils.createDestinationWithCertificateStatus(
              'BarDevice', 'BarName', false);
      const recentDestination =
          print_preview.makeRecentDestination(cloudPrinter);
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [recentDestination],
      });
      initialSettings.userAccounts = ['foo@chromium.org'];

      return setInitialSettings().then(function(args) {
        assertEquals('FooDevice', args.destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, args.type);
        assertEquals('FooDevice', destinationStore.selectedDestination.id);
      });
    });

    /**
     * Tests that if the user has a recent destination that is already in the
     * store (PDF printer), the DestinationStore does not try to select a
     * printer again later. Regression test for https://crbug.com/927162.
     */
    test(assert(TestNames.RecentSaveAsPdf), function() {
      const pdfPrinter = print_preview_test_utils.getSaveAsPdfDestination();
      const recentDestination = print_preview.makeRecentDestination(pdfPrinter);
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [recentDestination],
      });

      print_preview.DestinationStore.AUTO_SELECT_TIMEOUT_ = 0;
      return setInitialSettings()
          .then(function() {
            assertEquals(
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
                destinationStore.selectedDestination.id);
            return new Promise(resolve => setTimeout(resolve));
          })
          .then(function() {
            // Should still have Save as PDF.
            assertEquals(
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
                destinationStore.selectedDestination.id);
          });
    });

    /**
     * Tests that if there are recent destinations from different accounts, only
     * destinations associated with the most recent account are fetched.
     */
    test(assert(TestNames.MultipleRecentDestinationsAccounts), function() {
      const account1 = 'foo@chromium.org';
      const account2 = 'bar@chromium.org';
      const driveUser1 =
          print_preview_test_utils.getGoogleDriveDestination(account1);
      const driveUser2 =
          print_preview_test_utils.getGoogleDriveDestination(account2);
      const cloudPrinterUser1 = new print_preview.Destination(
          'FooCloud', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooCloudName',
          print_preview.DestinationConnectionStatus.ONLINE,
          {account: account1});
      const recentDestinations = [
        print_preview.makeRecentDestination(driveUser1),
        print_preview.makeRecentDestination(driveUser2),
        print_preview.makeRecentDestination(cloudPrinterUser1),
      ];
      cloudDestinations = [driveUser1, driveUser2, cloudPrinterUser1];
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });
      initialSettings.userAccounts = [account1, account2];
      initialSettings.syncAvailable = true;

      return setInitialSettings().then(() => {
        // Should have loaded Google Drive as the selected printer, since it
        // was most recent.
        assertEquals(
            print_preview.Destination.GooglePromotedId.DOCS,
            destinationStore.selectedDestination.id);

        // Only the most recent printer + Save as PDF are in the store.
        const loadedPrintersAccount1 = destinationStore.destinations(account1);
        assertEquals(2, loadedPrintersAccount1.length);
        cloudDestinations.forEach((destination) => {
          assertEquals(
              destination === driveUser1,
              loadedPrintersAccount1.some(p => p.key == destination.key));
        });
        assertEquals(1, numPrintersSelected);

        // Only Save as PDF exists when filtering for account 2.
        const loadedPrintersAccount2 = destinationStore.destinations(account2);
        assertEquals(1, loadedPrintersAccount2.length);
        assertEquals(
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
            loadedPrintersAccount2[0].id);
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
