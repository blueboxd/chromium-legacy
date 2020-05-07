// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the CROSTINI_EXPORT_IMPORT
 * route.
 * Chrome OS only.
 */

GEN_INCLUDE([
  'crostini_accessibility_test.js',
  'os_settings_accessibility_test.js',
]);

GEN('#include "content/public/test/browser_test.h"');

AccessibilityTest.define('CrostiniAccessibilityTest', {
  /** @override */
  name: 'CROSTINI_EXPORT_IMPORT',
  /** @override */
  axeOptions: OSSettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    settings.Router.getInstance().navigateTo(
        settings.routes.CROSTINI_EXPORT_IMPORT);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: OSSettingsAccessibilityTest.violationFilter,
});
