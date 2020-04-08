// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings interactive UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "chrome/browser/ui/ui_features.h"');

/**
 * Test fixture for interactive Polymer Settings elements.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
function CrSettingsInteractiveUITest() {}

CrSettingsInteractiveUITest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  },

  /** @override */
  extraLibraries: [
    ...PolymerInteractiveUITest.prototype.extraLibraries,
    'ensure_lazy_loaded.js',
  ],

  /** @override */
  get featureList() {
    return {disabled: ['features::kSettingsPolymer3']};
  },

  /** @override */
  setUp: function() {
    PolymerInteractiveUITest.prototype.setUp.call(this);

    settings.ensureLazyLoaded();
  },
};


/**
 * Test fixture for Sync Page.
 * @constructor
 * @extends {CrSettingsInteractiveUITest}
 */
function CrSettingsSyncPageTest() {}

CrSettingsSyncPageTest.prototype = {
  __proto__: CrSettingsInteractiveUITest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/people_page/sync_page.html',

  /** @override */
  extraLibraries: CrSettingsInteractiveUITest.prototype.extraLibraries.concat([
    '../test_browser_proxy.js',
    'sync_test_util.js',
    'test_sync_browser_proxy.js',
    'people_page_sync_page_interactive_test.js',
  ]),
};

TEST_F('CrSettingsSyncPageTest', 'All', function() {
  mocha.run();
});


/**
 * @constructor
 * @extends {CrSettingsInteractiveUITest}
 */
function CrSettingsAnimatedPagesTest() {}

CrSettingsAnimatedPagesTest.prototype = {
  __proto__: CrSettingsInteractiveUITest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/settings_page/settings_animated_pages.html',

  extraLibraries: CrSettingsInteractiveUITest.prototype.extraLibraries.concat([
    '../test_util.js',
    'test_util.js',
    'settings_animated_pages_test.js',
  ]),
};

TEST_F('CrSettingsAnimatedPagesTest', 'All', function() {
  mocha.run();
});


/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSecureDnsTest() {}

CrSettingsSecureDnsTest.prototype = {
  __proto__: CrSettingsInteractiveUITest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/privacy_page/secure_dns.html',

  /** @override */
  extraLibraries: CrSettingsInteractiveUITest.prototype.extraLibraries.concat([
    '../test_util.js',
    '../test_browser_proxy.js',
    'test_privacy_page_browser_proxy.js',
    'secure_dns_interactive_test.js',
  ]),
};

TEST_F('CrSettingsSecureDnsTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsInteractiveUITest}
 */
function SettingsUIInteractiveTest() {}

SettingsUIInteractiveTest.prototype = {
  __proto__: CrSettingsInteractiveUITest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/settings_ui/settings_ui.html',

  /** @override */
  extraLibraries: CrSettingsInteractiveUITest.prototype.extraLibraries.concat([
    '../test_util.js',
    'settings_ui_tests.js',
  ]),
};

// Flaky on Mac (see https://crbug.com/1065154).
GEN('#if !defined(OS_MACOSX)');
TEST_F('SettingsUIInteractiveTest', 'All', function() {
  mocha.run();
});
GEN('#endif');
