// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeLayer, PluginProxy} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {PDFPluginStub} from 'chrome://test/print_preview/plugin_stub.js';
import {getCddTemplate, getDefaultInitialSettings} from 'chrome://test/print_preview/print_preview_test_utils.js';

window.policy_tests = {};
policy_tests.suiteName = 'PolicyTest';
/** @enum {string} */
policy_tests.TestNames = {
  HeaderFooterPolicy: 'header/footer policy',
};

suite(policy_tests.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /**
   * @param {!NativeInitialSettings} initialSettings
   * @return {!Promise} A Promise that resolves once initial settings are done
   *     loading.
   */
  function loadInitialSettings(initialSettings) {
    PolymerTest.clearBody();
    const nativeLayer = new NativeLayerStub();
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplate(initialSettings.printerName));
    nativeLayer.setPageCount(3);
    NativeLayer.setInstance(nativeLayer);
    const pluginProxy = new PDFPluginStub();
    PluginProxy.setInstance(pluginProxy);

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);

    // Wait for initialization to complete.
    return Promise
        .all([
          nativeLayer.whenCalled('getInitialSettings'),
          nativeLayer.whenCalled('getPrinterCapabilities')
        ])
        .then(function() {
          flush();
        });
  }

  /**
   * Sets up the Print Preview app, and loads initial settings with the given
   * policies.
   * @param {boolean|undefined} allowedMode Allowed value for the header/footer
   *     setting.
   * @param {boolean|undefined} defaultMode Default value for the header/footer
   *     setting.
   * @return {!Promise} A Promise that resolves once initial settings are done
   *     loading.
   */
  function doHeaderFooterSetup(allowedMode, defaultMode) {
    const initialSettings = getDefaultInitialSettings();

    if (allowedMode !== undefined || defaultMode !== undefined) {
      const headerFooterPolicy = {};
      if (allowedMode !== undefined) {
        headerFooterPolicy.allowedMode = allowedMode;
      }
      if (defaultMode !== undefined) {
        headerFooterPolicy.defaultMode = defaultMode;
      }
      initialSettings.policies = {headerFooter: headerFooterPolicy};
    }
    // We want to make sure sticky settings get overridden.
    initialSettings.serializedAppStateStr = JSON.stringify({
      version: 2,
      isHeaderFooterEnabled: !defaultMode,
    });
    return loadInitialSettings(initialSettings);
  }

  function toggleMoreSettings() {
    const moreSettingsElement =
        page.$$('print-preview-sidebar').$$('print-preview-more-settings');
    moreSettingsElement.$.label.click();
  }

  function getHeaderFooterCheckbox() {
    return page.$$('print-preview-sidebar')
        .$$('print-preview-other-options-settings')
        .$$('#headerFooter');
  }

  /**
   * Tests different scenarios of applying header/footer policy.
   */
  test(assert(policy_tests.TestNames.HeaderFooterPolicy), function() {
    [{
      // No policies.
      allowedMode: undefined,
      defaultMode: undefined,
      expectedDisabled: false,
      expectedChecked: false,
    },
     {
       // Restrict header/footer to be enabled.
       allowedMode: true,
       defaultMode: undefined,
       expectedDisabled: true,
       expectedChecked: true,
     },
     {
       // Restrict header/footer to be disabled.
       allowedMode: false,
       defaultMode: undefined,
       expectedDisabled: true,
       expectedChecked: false,
     },
     {
       // Check header/footer checkbox.
       allowedMode: undefined,
       defaultMode: true,
       expectedDisabled: false,
       expectedChecked: true,
     },
     {
       // Uncheck header/footer checkbox.
       allowedMode: undefined,
       defaultMode: false,
       expectedDisabled: false,
       expectedChecked: false,
     }].forEach(subtestParams => {
      doHeaderFooterSetup(subtestParams.allowedMode, subtestParams.defaultMode)
          .then(function() {
            toggleMoreSettings();
            const checkbox = getHeaderFooterCheckbox();
            assertEquals(subtestParams.expectedDisabled, checkbox.disabled);
            assertEquals(subtestParams.expectedChecked, checkbox.checked);
          });
    });
  });
});
