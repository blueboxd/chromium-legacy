// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MANAGE_ACCESSIBILITY route.
 * Chrome OS only.
 */

// OSSettingsAccessibilityTest fixture.
GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  'os_settings_accessibility_test.js',
]);

GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var ManageAccessibilityA11yTest = class extends OSSettingsAccessibilityTest {};

AccessibilityTest.define('ManageAccessibilityA11yTest', {
  /** @override */
  name: 'MANAGE_ACCESSIBILITY',
  /** @override */
  axeOptions: OSSettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_ACCESSIBILITY);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: OSSettingsAccessibilityTest.violationFilter,
});
