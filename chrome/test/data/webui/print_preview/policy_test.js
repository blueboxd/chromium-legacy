// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('policy_tests', function() {
  /** @enum {string} */
  const TestNames = {
    EnableHeaderFooterByPref: 'enable header and footer by pref',
    DisableHeaderFooterByPref: 'disable header and footer by pref',
    EnableHeaderFooterByPolicy: 'enable header and footer by policy',
    DisableHeaderFooterByPolicy: 'disable header and footer by policy',
  };

  const suiteName = 'PolicyTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /**
     * @param {!print_preview.NativeInitialSettings} initialSettings
     * @return {!Promise} A Promise that resolves once initial settings are done
     *     loading.
     */
    function loadInitialSettings(initialSettings) {
      const nativeLayer = new print_preview.NativeLayerStub();
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinationCapabilities(
          print_preview_test_utils.getCddTemplate(initialSettings.printerName));
      nativeLayer.setPageCount(3);
      print_preview.NativeLayer.setInstance(nativeLayer);
      const pluginProxy = new print_preview.PDFPluginStub();
      print_preview.PluginProxy.setInstance(pluginProxy);

      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);
      const previewArea = page.$.previewArea;

      // Wait for initialization to complete.
      return Promise
          .all([
            nativeLayer.whenCalled('getInitialSettings'),
            nativeLayer.whenCalled('getPrinterCapabilities')
          ])
          .then(function() {
            Polymer.dom.flush();
          });
    }

    /**
     * Sets up the Print Preview app, and loads initial settings with the given
     * prefs/policies.
     * @param headerFooter {boolean} Value for the
     *     'printing.print_header_footer' pref.
     * @param isHeaderFooterManaged {boolean} Whether the
     *     'printing.print_header_footer' is controlled by an enterprise policy.
     * @return {!Promise} A Promise that resolves once initial settings are done
     *     loading.
     */
    function doSetup(headerFooter, isHeaderFooterManaged) {
      const initialSettings =
          print_preview_test_utils.getDefaultInitialSettings();
      initialSettings.headerFooter = headerFooter;
      initialSettings.isHeaderFooterManaged = isHeaderFooterManaged;
      // We want to make sure sticky settings get overridden.
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        isHeaderFooterEnabled: !headerFooter,
      });
      return loadInitialSettings(initialSettings);
    }

    function toggleMoreSettings() {
      const moreSettingsElement =
          page.$$('print-preview-sidebar').$$('print-preview-more-settings');
      moreSettingsElement.$.label.click();
    }

    function getCheckbox() {
      return page.$$('print-preview-sidebar')
          .$$('print-preview-other-options-settings')
          .$$('#headerFooter');
    }

    /**
     * Tests that 'printing.print_header_footer' pref checks the header/footer
     * checkbox.
     */
    test(assert(TestNames.EnableHeaderFooterByPref), function() {
      doSetup(true, false).then(function() {
        toggleMoreSettings();
        assertFalse(getCheckbox().disabled);
        assertTrue(getCheckbox().checked);
      });
    });

    /**
     * Tests that 'printing.print_header_footer' pref unchecks the header/footer
     * checkbox.
     */
    test(assert(TestNames.DisableHeaderFooterByPref), function() {
      doSetup(false, false).then(function() {
        toggleMoreSettings();
        assertFalse(getCheckbox().disabled);
        assertFalse(getCheckbox().checked);
      });
    });

    /**
     * Tests that 'force enable header/footer' policy disables the header/footer
     * checkbox and checks it.
     */
    test(assert(TestNames.EnableHeaderFooterByPolicy), function() {
      doSetup(true, true).then(function() {
        toggleMoreSettings();
        assertTrue(getCheckbox().disabled);
        assertTrue(getCheckbox().checked);
      });
    });

    /**
     * Tests that 'force enable header/footer' policy disables the header/footer
     * checkbox and unchecks it.
     */
    test(assert(TestNames.DisableHeaderFooterByPolicy), function() {
      doSetup(false, true).then(function() {
        toggleMoreSettings();
        assertTrue(getCheckbox().disabled);
        assertFalse(getCheckbox().checked);
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
