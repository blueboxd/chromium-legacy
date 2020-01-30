// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */
GEN('#include "chrome/common/buildflags.h"');

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

// Settings v3 resources are only available in non optimized builds.
GEN('#if !BUILDFLAG(OPTIMIZE_WEBUI)');
/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var CrSettingsV3BrowserTest = class extends PolymerTest {
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
var CrControlledButtonV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/controlled_button_tests.m.js';
  }
};

TEST_F('CrControlledButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrControlledRadioButtonV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/controlled_radio_button_tests.m.js';
  }
};

TEST_F('CrControlledRadioButtonV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsCheckboxV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/checkbox_tests.m.js';
  }
};

TEST_F('CrSettingsCheckboxV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsDropdownMenuV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/dropdown_menu_tests.m.js';
  }
};

TEST_F('CrSettingsDropdownMenuV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsExtensionControlledIndicatorV3Test =
    class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/extension_controlled_indicator_tests.m.js';
  }
};

TEST_F('CrSettingsExtensionControlledIndicatorV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPrefUtilV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/pref_util_tests.m.js';
  }
};

TEST_F('CrSettingsPrefUtilV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSliderV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_slider_tests.m.js';
  }
};

TEST_F('CrSettingsSliderV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsTextareaV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_textarea_tests.m.js';
  }
};

TEST_F('CrSettingsTextareaV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsToggleButtonV3Test = class extends CrSettingsV3BrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_toggle_button_tests.m.js';
  }
};

TEST_F('CrSettingsToggleButtonV3Test', 'All', function() {
  mocha.run();
});

GEN('#endif  // !BUILDFLAG(OPTIMIZE_WEBUI)');
