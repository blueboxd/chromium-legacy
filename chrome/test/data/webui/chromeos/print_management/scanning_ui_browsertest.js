// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://scanning.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chromeos/constants/chromeos_features.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function ScanningUIBrowserTest() {}

ScanningUIBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://scanning/test_loader.html?module=chromeos/' +
      'print_management/scanning_ui_test.js',

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],

  featureList: {
    enabled: [
      'chromeos::features::kScanningUI',
    ]
  },
};

TEST_F('ScanningUIBrowserTest', 'All', function() {
  mocha.run();
});
