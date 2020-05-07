// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var CrSettingsV3InteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

// eslint-disable-next-line no-var
var CrSettingsAnimatedPagesV3Test =
    class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_animated_pages_test.js';
  }
};

TEST_F('CrSettingsAnimatedPagesV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSyncPageV3Test = class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/people_page_sync_page_interactive_test.js';
  }
};

TEST_F('CrSettingsSyncPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSecureDnsV3Test = class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/secure_dns_interactive_test.js';
  }
};

TEST_F('CrSettingsSecureDnsV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var SettingsUIV3InteractiveTest = class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_ui_tests.js';
  }
};

// Times out on Mac. See https://crbug.com/1060981.
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_SettingsUIV3 DISABLED_SettingsUIV3');
GEN('#else');
GEN('#define MAYBE_SettingsUIV3 SettingsUIV3');
GEN('#endif');
TEST_F('SettingsUIV3InteractiveTest', 'MAYBE_SettingsUIV3', function() {
  mocha.run();
});
