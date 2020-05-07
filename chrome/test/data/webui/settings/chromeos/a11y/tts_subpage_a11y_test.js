// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MANAGE_TTS_SETTINGS route.
 * Chrome OS only.
 */

// OSSettingsAccessibilityTest fixture.
GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  'os_settings_accessibility_test.js',
]);

GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var TtsAccessibilityTest = class extends OSSettingsAccessibilityTest {
  /** @override */
  get commandLineSwitches() {
    return ['enable-experimental-a11y-features'];
  }
};

AccessibilityTest.define('TtsAccessibilityTest', {
  /** @override */
  name: 'MANAGE_TTS_SETTINGS',
  /** @override */
  axeOptions: OSSettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_TTS_SETTINGS);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: OSSettingsAccessibilityTest.violationFilter,
});
