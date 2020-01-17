// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MULTIDEVICE route.
 * Chrome OS only.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  'settings_accessibility_test.js',
]);

// eslint-disable-next-line no-var
var MultideviceA11yTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }
};

AccessibilityTest.define('MultideviceA11yTest', {
  /** @override */
  name: 'MULTIDEVICE',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptionsExcludeLinkInTextBlock,
  /** @override */
  setup: function() {
    settings.Router.getInstance().navigateTo(settings.routes.MULTIDEVICE);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});
