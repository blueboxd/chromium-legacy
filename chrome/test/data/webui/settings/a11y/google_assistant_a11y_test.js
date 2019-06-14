// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the GOOGLE_ASSISTANT route.
 * Chrome OS only.
 */

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

GEN('#include "chromeos/constants/chromeos_switches.h"');

// eslint-disable-next-line no-var
var GoogleAssistantAccessibilityTest = class extends SettingsAccessibilityTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::switches::kAssistantFeature']};
  }
};

AccessibilityTest.define('GoogleAssistantAccessibilityTest', {
  /** @override */
  name: 'GOOGLE_ASSISTANT',
  /** @override */
  axeOptions: GoogleAssistantAccessibilityTest.axeOptions,
  /** @override */
  violationFilter: GoogleAssistantAccessibilityTest.violationFilter,

  /** @override */
  setup: function() {
    settings.router.navigateTo(settings.routes.GOOGLE_ASSISTANT);
    Polymer.dom.flush();
  },

  /** @override */
  tests: {'Accessible with No Changes': function() {}},
});
