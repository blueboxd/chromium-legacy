// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://print-management scanning page.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function ScanningPageBrowserTest() {}

ScanningPageBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://print-management/test_loader.html?module=chromeos/' +
      'print_management/scanning_page_test.js',

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],

  featureList: {
    enabled: [
      'chromeos::features::kPrintJobManagementApp',
      'chromeos::features::kScanningUI',
    ]
  },
};

TEST_F('ScanningPageBrowserTest', 'All', function() {
  mocha.run();
});
