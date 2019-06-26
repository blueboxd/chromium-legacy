// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_button_test', function() {
  /** @enum {string} */
  const TestNames = {
    LocalPrintHidePreview: 'local print hide preview',
    PDFPrintVisiblePreview: 'pdf print visible preview',
  };

  const suiteName = 'PrintButtonTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {boolean} */
    let printBeforePreviewReady = false;

    /** @type {boolean} */
    let previewHidden = false;

    /** @type {!print_preview.NativeInitialSettings} */
    const initialSettings =
        print_preview_test_utils.getDefaultInitialSettings();

    /** @override */
    setup(function() {
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();
      nativeLayer.setInitialSettings(initialSettings);
      let localDestinationInfos = [
        {printerName: 'FooName', deviceName: 'FooDevice'},
      ];
      nativeLayer.setLocalDestinations(localDestinationInfos);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(
              initialSettings.printerName));
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getPdfPrinter());

      const pluginProxy = new print_preview.PDFPluginStub();
      pluginProxy.setPluginCompatible(true);
      print_preview.PluginProxy.setInstance(pluginProxy);

      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      pluginProxy.setPreloadCallback(() => {
        // Print before calling previewArea.onPluginLoad_. This simulates the
        // user clicking the print button while the preview is still loading,
        // since previewArea.onPluginLoad_() indicates to the UI that the
        // preview is ready.
        if (printBeforePreviewReady) {
          const sidebar = page.$$('print-preview-sidebar');
          const parentElement =
              loadTimeData.getBoolean('newPrintPreviewLayoutEnabled') ?
              sidebar.$$('print-preview-button-strip') :
              sidebar.$$('print-preview-header');
          const printButton = parentElement.$$('.action-button');
          assertFalse(printButton.disabled);
          printButton.click();
        }
      });

      previewHidden = false;
      nativeLayer.whenCalled('hidePreview').then(() => {
        previewHidden = true;
      });
    });

    function waitForInitialPreview() {
      return nativeLayer.whenCalled('getInitialSettings')
          .then(function() {
            // Wait for the preview request.
            return Promise.all([
              nativeLayer.whenCalled('getPrinterCapabilities'),
              nativeLayer.whenCalled('getPreview')
            ]);
          });
    }

    // Tests that hidePreview() is called before print() if a local printer is
    // selected and the user clicks print while the preview is loading.
    test(assert(TestNames.LocalPrintHidePreview), function() {
      printBeforePreviewReady = true;

      return waitForInitialPreview().then(function() {
        // Wait for the print request.
        return nativeLayer.whenCalled('print');
      }).then(function(printTicket) {
        assertTrue(previewHidden);

        // Verify that the printer name is correct.
        assertEquals('FooDevice', JSON.parse(printTicket).deviceName);
        return nativeLayer.whenCalled('dialogClose');
      });
    });

    // Tests that hidePreview() is not called if Save as PDF is selected and
    // the user clicks print while the preview is loading.
    test(assert(TestNames.PDFPrintVisiblePreview), function() {
      printBeforePreviewReady = false;

      return waitForInitialPreview()
          .then(function() {
            nativeLayer.reset();
            // Setup to print before the preview loads.
            printBeforePreviewReady = true;

            // Select Save as PDF destination
            const destinationSettings =
                page.$$('print-preview-sidebar')
                    .$$('print-preview-destination-settings');
            const pdfDestination =
                destinationSettings.destinationStore_.destinations().find(
                    d => d.id == 'Save as PDF');
            assertTrue(!!pdfDestination);
            destinationSettings.destinationStore_.selectDestination(
                pdfDestination);

            // Reload preview and wait for print.
            return nativeLayer.whenCalled('print');
          })
          .then(function(printTicket) {
            assertFalse(previewHidden);

            // Verify that the printer name is correct.
            assertEquals('Save as PDF', JSON.parse(printTicket).deviceName);
            return nativeLayer.whenCalled('dialogClose');
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
