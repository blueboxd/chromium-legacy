// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/common/buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var OSSettingsV3BrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'network::features::kOutOfBlinkCors',
      ],
    };
  }
};

[['ResetPage', 'os_reset_page_test.m.js'],
 ['LocalizedLink', 'localized_link_test.m.js'],
 ['BluetoothPage', 'bluetooth_page_tests.m.js'],
 ['NearbyShareSubPage', 'nearby_share_subpage_tests.m.js'],
 ['MultidevicePage', 'multidevice_page_tests.m.js'],
 ['MultideviceSubPage', 'multidevice_subpage_tests.m.js'],
 ['MultideviceSmartLockSubPage', 'multidevice_smartlock_subpage_test.m.js'],
 ['MultideviceFeatureItem', 'multidevice_feature_item_tests.m.js'],
 ['MultideviceFeatureToggle', 'multidevice_feature_toggle_tests.m.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `OSSettings${testName}V3Test`;
  this[className] = class extends OSSettingsV3BrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://os-settings/test_loader.html?module=settings/chromeos/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
