// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('invalid_settings_browsertest', function() {
  /** @enum {string} */
  const TestNames = {
    NoPDFPluginError: 'no pdf plugin error',
    InvalidSettingsError: 'invalid settings error',
    InvalidCertificateError: 'invalid certificate error',
    InvalidCertificateErrorReselectDestination: 'invalid certificate reselect',
  };

  const suiteName = 'InvalidSettingsBrowserTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {?cloudprint.CloudPrintInterface} */
    let cloudPrintInterface = null;

    /** @type {!print_preview.NativeInitialSettings} */
    const initialSettings = {
      isInKioskAutoPrintMode: false,
      isInAppKioskMode: false,
      thousandsDelimeter: ',',
      decimalDelimeter: '.',
      unitType: 1,
      previewModifiable: true,
      documentTitle: 'title',
      documentHasSelection: true,
      shouldPrintSelectionOnly: false,
      printerName: 'FooDevice',
      serializedAppStateStr: null,
      serializedDefaultDestinationSelectionRulesStr: null
    };

    /** @type {!Array<!print_preview.LocalDestinationInfo>} */
    let localDestinationInfos = [
      {printerName: 'FooName', deviceName: 'FooDevice'},
      {printerName: 'BarName', deviceName: 'BarDevice'},
    ];

    /** @override */
    setup(function() {
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      cloudPrintInterface = new print_preview.CloudPrintInterfaceStub();
      cloudprint.setCloudPrintInterfaceForTesting(cloudPrintInterface);
      PolymerTest.clearBody();
    });

    /**
     * Initializes the page with initial settings and local destinations
     * given by |initialSettings| and |localDestinationInfos|. Also creates
     * the fake plugin. Moved out of setup so tests can set those parameters
     * differently.
     * @param {boolean} pluginCompatible Whether the plugin should be set to
     *     appear compatible.
     */
    function createPage(pluginCompatible) {
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinations(localDestinationInfos);
      if (initialSettings.printerName) {
        nativeLayer.setLocalDestinationCapabilities(
            print_preview_test_utils.getCddTemplate(
                initialSettings.printerName));
      }
      const pluginProxy = new print_preview.PDFPluginStub();
      pluginProxy.setPluginCompatible(pluginCompatible);
      print_preview.PluginProxy.setInstance(pluginProxy);

      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      page.$.documentInfo.init(true, 'title', false);
      const previewArea = page.$.previewArea;
    }

    /**
     * Performs some setup for invalid certificate tests using 2 destinations
     * in |printers|. printers[0] will be set as the most recent destination,
     * and printers[1] will be the second most recent destination. Sets up
     * cloud print interface, user info, and runs createPage().
     * @param {!Array<!print_preview.Destination>} printers
     */
    function setupInvalidCertificateTest(printers) {
      initialSettings.printerName = '';
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [
          print_preview.makeRecentDestination(printers[0]),
          print_preview.makeRecentDestination(printers[1]),
        ],
      });
      initialSettings.cloudPrintURL = 'cloudprint URL';
      initialSettings.userAccounts = [printers[0].account];
      localDestinationInfos = [];

      loadTimeData.overrideValues({isEnterpriseManaged: false});
      createPage(true);

      printers.forEach(printer => cloudPrintInterface.setPrinter(printer));
    }

    // Test that error message is displayed when plugin doesn't exist.
    // TODO (rbpotter): Fix this test so that it works again with calling
    // appendChild() before setting checkPluginCompatibility.
    test(assert(TestNames.NoPDFPluginError), function() {
      createPage(false);
      const previewArea = page.$.previewArea;

      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        const overlayEl = previewArea.$$('.preview-area-overlay-layer');
        const messageEl = previewArea.$$('.preview-area-message');
        assertEquals(print_preview.State.FATAL_ERROR, page.state);

        // Make sure the overlay is visible.
        assertFalse(overlayEl.classList.contains('invisible'));

        // Make sure the correct text is shown.
        const expectedMessageChromium = 'Chromium cannot show the print ' +
            'preview when the built-in PDF viewer is missing.';
        const expectedMessageChrome = 'Google Chrome cannot show the print ' +
            'preview when the built-in PDF viewer is missing.';
        assertTrue(
            messageEl.textContent.includes(expectedMessageChromium) ||
            messageEl.textContent.includes(expectedMessageChrome));
      });
    });

    // Tests that when a printer cannot be communicated with correctly the
    // preview area displays an invalid printer error message and printing
    // is disabled. Verifies that the user can recover from this error by
    // selecting a different, valid printer.
    test(assert(TestNames.InvalidSettingsError), function() {
      createPage(true);
      const barDevice = print_preview_test_utils.getCddTemplate('BarDevice');
      nativeLayer.setLocalDestinationCapabilities(barDevice);

      // FooDevice is the default printer, so will be selected for the initial
      // preview request.
      nativeLayer.setInvalidPrinterId('FooDevice');

      // Expected message
      const expectedMessage = 'The selected printer is not available or not ' +
          'installed correctly.  Check your printer or try selecting another ' +
          'printer.';

      // Get references to relevant elements.
      const previewAreaEl = page.$.previewArea;
      const overlay = previewAreaEl.$$('.preview-area-overlay-layer');
      const messageEl = previewAreaEl.$$('.preview-area-message');
      const sidebar = page.$$('print-preview-sidebar');
      let printButton = null;
      const destinationSettings =
          sidebar.$$('print-preview-destination-settings');

      return test_util.waitForRender(page)
          .then(() => {
            const parentElement =
                loadTimeData.getBoolean('newPrintPreviewLayoutEnabled') ?
                sidebar.$$('print-preview-button-strip') :
                sidebar.$$('print-preview-header');
            printButton = parentElement.$$('.action-button');

            return Promise.all([
              print_preview.Model.whenReady(),
              nativeLayer.whenCalled('getInitialSettings'),
            ]);
          })
          .then(function() {
            destinationSettings.destinationStore_.startLoadAllDestinations();
            // Wait for the preview request.
            return Promise.all([
              nativeLayer.whenCalled('getPrinterCapabilities'),
              nativeLayer.whenCalled('getPreview')
            ]);
          })
          .then(function() {
            // Print preview should have failed with invalid settings, since
            // FooDevice was set as an invalid printer.
            assertFalse(overlay.classList.contains('invisible'));
            assertTrue(messageEl.textContent.includes(expectedMessage));
            assertEquals(print_preview.State.ERROR, page.state);

            // Verify that the print button is disabled
            assertTrue(printButton.disabled);

            // Select should still be enabled so that the user can select a new
            // printer.
            assertFalse(destinationSettings.$.destinationSelect.disabled);

            // Reset
            nativeLayer.reset();

            // Select a new destination
            const barDestination =
                destinationSettings.destinationStore_.destinations().find(
                    d => d.id == 'BarDevice');
            destinationSettings.destinationStore_.selectDestination(
                barDestination);

            // Wait for the preview to be updated.
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function() {
            // Message should be gone.
            assertTrue(overlay.classList.contains('invisible'));
            assertFalse(messageEl.textContent.includes(expectedMessage));

            // Has active print button and successfully 'prints', indicating
            assertFalse(printButton.disabled);
            assertEquals(print_preview.State.READY, page.state);
            printButton.click();
            // This should result in a call to print.
            return nativeLayer.whenCalled('print');
          })
          .then(
              /**
               * @param {string} printTicket The print ticket print() was
               *     called for.
               */
              function(printTicket) {
                // Sanity check some printing argument values.
                const ticket = JSON.parse(printTicket);
                assertEquals(barDevice.printer.deviceName, ticket.deviceName);
                assertEquals(
                    print_preview_test_utils.getDefaultOrientation(barDevice) ==
                        'LANDSCAPE',
                    ticket.landscape);
                assertEquals(1, ticket.copies);
                const mediaDefault =
                    print_preview_test_utils.getDefaultMediaSize(barDevice);
                assertEquals(
                    mediaDefault.width_microns, ticket.mediaSize.width_microns);
                assertEquals(
                    mediaDefault.height_microns,
                    ticket.mediaSize.height_microns);
                return nativeLayer.whenCalled('dialogClose');
              });
    });

    // Test that GCP invalid certificate printers disable the print preview when
    // selected and display an error and that the preview dialog can be
    // recovered by selecting a new destination. Verifies this works when the
    // invalid printer is the most recent destination and is selected by
    // default.
    test(assert(TestNames.InvalidCertificateError), function() {
      const invalidPrinter =
          print_preview_test_utils.createDestinationWithCertificateStatus(
              'FooDevice', 'FooName', true);
      const validPrinter =
          print_preview_test_utils.createDestinationWithCertificateStatus(
              'BarDevice', 'BarName', false);
      setupInvalidCertificateTest([invalidPrinter, validPrinter]);

      // Expected message
      const expectedMessageStart = 'The selected Google Cloud Print device ' +
          'is no longer supported.';
      const expectedMessageEnd = 'Try setting up the printer in your ' +
          'computer\'s system settings.';

      // Get references to relevant elements.
      const previewAreaEl = page.$.previewArea;
      const overlayEl = previewAreaEl.$$('.preview-area-overlay-layer');
      const messageEl = previewAreaEl.$$('.preview-area-message');
      const sidebar = page.$$('print-preview-sidebar');
      let printButton = null;
      const destinationSettings =
          sidebar.$$('print-preview-destination-settings');
      const scalingSettings = sidebar.$$('print-preview-scaling-settings')
                                  .$$('print-preview-number-settings-section');
      const layoutSettings = sidebar.$$('print-preview-layout-settings');

      return test_util.waitForRender(page)
          .then(() => {
            const parentElement =
                loadTimeData.getBoolean('newPrintPreviewLayoutEnabled') ?
                sidebar.$$('print-preview-button-strip') :
                sidebar.$$('print-preview-header');
            printButton = parentElement.$$('.action-button');
            return Promise.all([
              print_preview.Model.whenReady(),
              nativeLayer.whenCalled('getInitialSettings'),
            ]);
          })
          .then(function() {
            // Set this to enable the scaling input.
            page.setSetting('customScaling', true);

            destinationSettings.destinationStore_.startLoadCloudDestinations();

            // FooDevice will be selected since it is the most recently used
            // printer, so the invalid certificate error should be shown.
            // The overlay must be visible for the message to be seen.
            assertFalse(overlayEl.classList.contains('invisible'));

            // Verify that the correct message is shown.
            assertTrue(messageEl.textContent.includes(expectedMessageStart));
            assertTrue(messageEl.textContent.includes(expectedMessageEnd));

            // Verify that the print button is disabled
            assertTrue(printButton.disabled);

            // Verify the state is invalid and that some settings sections are
            // also disabled, so there is no way to regenerate the preview.
            assertEquals(print_preview.State.ERROR, page.state);
            assertTrue(layoutSettings.$$('select').disabled);
            assertTrue(scalingSettings.$$('cr-input').disabled);

            // The destination select dropdown should be enabled, so that the
            // user can select a new printer.
            assertFalse(destinationSettings.$.destinationSelect.disabled);

            // Reset
            nativeLayer.reset();

            // Select a new, valid cloud destination.
            destinationSettings.destinationStore_.selectDestination(
                validPrinter);

            return nativeLayer.whenCalled('getPreview');
          })
          .then(function() {
            // Has active print button, indicating recovery from error state.
            assertFalse(printButton.disabled);
            assertEquals(print_preview.State.READY, page.state);

            // Settings sections are now active.
            assertFalse(layoutSettings.$$('select').disabled);
            assertFalse(scalingSettings.$$('cr-input').disabled);

            // The destination select dropdown should still be enabled.
            assertFalse(destinationSettings.$.destinationSelect.disabled);

            // Message text should have changed and overlay should be
            // invisible.
            assertFalse(messageEl.textContent.includes(expectedMessageStart));
            assertTrue(overlayEl.classList.contains('invisible'));
          });
    });

    // Test that GCP invalid certificate printers disable the print preview when
    // selected and display an error and that the preview dialog can be
    // recovered by selecting a new destination. Tests that even if destination
    // was previously selected, the error is cleared.
    test(
        assert(TestNames.InvalidCertificateErrorReselectDestination),
        function() {
          const invalidPrinter =
              print_preview_test_utils.createDestinationWithCertificateStatus(
                  'FooDevice', 'FooName', true);
          const validPrinter =
              print_preview_test_utils.createDestinationWithCertificateStatus(
                  'BarDevice', 'BarName', false);
          setupInvalidCertificateTest([validPrinter, invalidPrinter]);

          // Get references to relevant elements.
          const previewAreaEl = page.$.previewArea;
          const overlayEl = previewAreaEl.$$('.preview-area-overlay-layer');
          const messageEl = previewAreaEl.$$('.preview-area-message');
          const sidebar = page.$$('print-preview-sidebar');
          let printButton = null;
          const destinationSettings =
              sidebar.$$('print-preview-destination-settings');

          return test_util.waitForRender(page)
              .then(() => {
                const parentElement =
                    loadTimeData.getBoolean('newPrintPreviewLayoutEnabled') ?
                    sidebar.$$('print-preview-button-strip') :
                    sidebar.$$('print-preview-header');
                printButton = parentElement.$$('.action-button');
                return Promise.all([
                  print_preview.Model.whenReady(),
                  nativeLayer.whenCalled('getInitialSettings'),
                ]);
              })
              .then(function() {
                // Start loading cloud destinations so that the printer
                // capabilities arrive.
                destinationSettings.destinationStore_
                    .startLoadCloudDestinations();
                return nativeLayer.whenCalled('getPreview');
              })
              .then(function() {
                // Has active print button.
                assertFalse(printButton.disabled);
                assertEquals(print_preview.State.READY, page.state);
                // No error message.
                assertTrue(overlayEl.classList.contains('invisible'));

                // Select the invalid destination and wait for the event.
                const whenInvalid = test_util.eventToPromise(
                    print_preview.DestinationStore.EventType.ERROR,
                    destinationSettings.destinationStore_);
                destinationSettings.destinationStore_.selectDestination(
                    invalidPrinter);
                return whenInvalid;
              })
              .then(function() {
                // Should have error message.
                assertFalse(overlayEl.classList.contains('invisible'));
                assertEquals(print_preview.State.ERROR, page.state);

                // Reset
                nativeLayer.reset();

                // Reselect the valid cloud destination.
                const whenSelected = test_util.eventToPromise(
                    print_preview.DestinationStore.EventType.DESTINATION_SELECT,
                    destinationSettings.destinationStore_);
                destinationSettings.destinationStore_.selectDestination(
                    validPrinter);
                return whenSelected;
              })
              .then(function() {
                // Has active print button and no error message.
                assertFalse(printButton.disabled);
                assertTrue(overlayEl.classList.contains('invisible'));
              });
        });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
