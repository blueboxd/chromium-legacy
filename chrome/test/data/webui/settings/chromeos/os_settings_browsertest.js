// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for Chrome OS settings page. */

// Path to general chrome browser settings and associated utilities.
const BROWSER_SETTINGS_PATH = '../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/public/cpp/ash_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

// Generic text fixture for CrOS Polymer Settings elements to be overridden by
// individual element tests.
const OSSettingsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat(
        [BROWSER_SETTINGS_PATH + 'ensure_lazy_loaded.js']);
  }

  /** @override */
  setUp() {
    super.setUp();
    settings.ensureLazyLoaded('chromeos');
  }
};

// Test fixture for the Smb Shares page.
// eslint-disable-next-line no-var
var OSSettingsSmbPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_downloads_page/smb_shares_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'smb_shares_page_tests.js',
    ]);
  }
};

// Settings tests are flaky on debug. See https://crbug.com/968608.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('OSSettingsSmbPageTest', 'MAYBE_All', function() {
  mocha.run();
});

// Test fixture for the main settings page.
// eslint-disable-next-line no-var
var OSSettingsMainTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_settings_main/os_settings_main.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'os_settings_main_test.js',
    ]);
  }
};

TEST_F('OSSettingsMainTest', 'MAYBE_All', function() {
  mocha.run();
});

// Tests for the side-nav menu.
// eslint-disable-next-line no-var
var OSSettingsMenuTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'os_settings_menu_test.js',
    ]);
  }
};

TEST_F('OSSettingsMenuTest', 'MAYBE_All', function() {
  mocha.run();
});

// Tests for the People section.
// eslint-disable-next-line no-var
var OSSettingsPeoplePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'sync_test_util.js',
      BROWSER_SETTINGS_PATH + 'test_profile_info_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_sync_browser_proxy.js',
      'os_people_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageTest', 'MAYBE_All', function() {
  mocha.run();
});
