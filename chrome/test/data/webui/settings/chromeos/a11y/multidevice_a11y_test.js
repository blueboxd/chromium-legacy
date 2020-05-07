// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MULTIDEVICE route.
 * Chrome OS only.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  'os_settings_accessibility_test.js',
]);

GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var MultideviceA11yTest = class extends OSSettingsAccessibilityTest {};

AccessibilityTest.define('MultideviceA11yTest', {
  /** @override */
  name: 'MULTIDEVICE',
  /** @override */
  axeOptions: OSSettingsAccessibilityTest.axeOptionsExcludeLinkInTextBlock,
  /** @override */
  setup: function() {
    settings.Router.getInstance().navigateTo(settings.routes.MULTIDEVICE);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: OSSettingsAccessibilityTest.violationFilter,
});
