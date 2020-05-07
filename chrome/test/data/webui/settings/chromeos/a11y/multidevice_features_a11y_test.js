// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MULTIDEVICE_FEATURES route.
 * Chrome OS only.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  'os_settings_accessibility_test.js',
]);

GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var MultideviceFeaturesA11yTest = class extends OSSettingsAccessibilityTest {};

AccessibilityTest.define('MultideviceFeaturesA11yTest', {
  /** @override */
  name: 'MULTIDEVICE_FEATURES_ACCESSIBILITY',
  /** @override */
  axeOptions: OSSettingsAccessibilityTest.axeOptionsExcludeLinkInTextBlock,
  /** @override */
  setup: function() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MULTIDEVICE_FEATURES);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter:
      Object.assign({}, OSSettingsAccessibilityTest.violationFilter, {
        // Excuse link without an underline.
        // TODO(https://crbug.com/894602): Remove this exception when settled
        // with UX.
        'link-in-text-block': function(nodeResult) {
          return nodeResult.element.parentElement.id === 'multideviceSubLabel';
        },
      }),
});
