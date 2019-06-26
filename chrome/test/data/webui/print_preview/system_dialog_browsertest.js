// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('system_dialog_browsertest', function() {
  /** @enum {string} */
  const TestNames = {
    LinkTriggersLocalPrint: 'link triggers local print',
    InvalidSettingsDisableLink: 'invalid settings disable link',
  };

  const suiteName = 'SystemDialogBrowserTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewSidebarElement} */
    let sidebar = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {?PrintPreviewLinkContainerElement} */
    let linkContainer = null;

    /** @type {?HTMLElement} */
    let link = null;

    /** @type {string} */
    let printTicketKey = '';

    /** @override */
    setup(function() {
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();

      const initialSettings =
          print_preview_test_utils.getDefaultInitialSettings();
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      const pluginProxy = new print_preview.PDFPluginStub();
      print_preview.PluginProxy.setInstance(pluginProxy);

      const page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;
      sidebar = page.$$('print-preview-sidebar');
      return Promise
          .all([
            test_util.waitForRender(page),
            print_preview.Model.whenReady(),
            nativeLayer.whenCalled('getInitialSettings'),
            nativeLayer.whenCalled('getPrinterCapabilities'),
          ])
          .then(function() {
            linkContainer = sidebar.$$('print-preview-link-container');
            return nativeLayer.whenCalled('getPreview');
          })
          .then(function() {
            assertEquals('FooDevice', page.destination_.id);
            link = cr.isWindows ? linkContainer.$.systemDialogLink :
                                  linkContainer.$.openPdfInPreviewLink;
            printTicketKey =
                cr.isWindows ? 'showSystemDialog' : 'OpenPDFInPreview';
          });
    });

    test(assert(TestNames.LinkTriggersLocalPrint), function() {
      assertFalse(linkContainer.disabled);
      assertFalse(link.hidden);
      link.click();
      // Should result in a print call and dialog should close.
      return nativeLayer.whenCalled('print').then(function(printTicket) {
        expectTrue(JSON.parse(printTicket)[printTicketKey]);
        return nativeLayer.whenCalled('dialogClose');
      });
    });

    test(assert(TestNames.InvalidSettingsDisableLink), function() {
      assertFalse(linkContainer.disabled);
      assertFalse(link.hidden);

      const moreSettingsElement = sidebar.$$('print-preview-more-settings');
      moreSettingsElement.$.label.click();
      const scalingSettings = sidebar.$$('print-preview-scaling-settings');
      assertFalse(scalingSettings.hidden);
      nativeLayer.resetResolver('getPreview');
      let previewCalls = 0;

      // Set scaling settings to custom.
      return print_preview_test_utils
          .selectOption(
              scalingSettings, scalingSettings.ScalingValue.CUSTOM.toString())
          .then(() => {
            previewCalls = nativeLayer.getCallCount('getPreview');

            // Set an invalid input.
            const scalingSettingsInput =
                scalingSettings.$$('print-preview-number-settings-section')
                    .$.userValue.inputElement;
            scalingSettingsInput.value = '0';
            scalingSettingsInput.dispatchEvent(
                new CustomEvent('input', {composed: true, bubbles: true}));

            return test_util.eventToPromise('input-change', scalingSettings);
          })
          .then(() => {
            // Expect disabled print button
            const parentElement =
                loadTimeData.getBoolean('newPrintPreviewLayoutEnabled') ?
                sidebar.$$('print-preview-button-strip') :
                sidebar.$$('print-preview-header');
            const printButton = parentElement.$$('.action-button');
            assertTrue(printButton.disabled);
            assertTrue(linkContainer.disabled);
            assertFalse(link.hidden);
            assertTrue(link.querySelector('cr-icon-button').disabled);

            // No new preview
            assertEquals(previewCalls, nativeLayer.getCallCount('getPreview'));
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
