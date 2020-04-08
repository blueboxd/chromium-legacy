// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Material Help page tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/ui_features.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsHelpPageBrowserTest() {}

SettingsHelpPageBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://help/',

  /** @override */
  featureList: {disabled: ['features::kSettingsPolymer3']},

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    'settings_page_test_util.js',
  ],

  /** @override */
  setUp: function() {
    // Intentionally bypassing SettingsPageBrowserTest#setUp.
    PolymerTest.prototype.setUp.call(this);
  },
};

TEST_F('SettingsHelpPageBrowserTest', 'Load', function() {
  // Register mocha tests.
  suite('Help page', function() {
    test('about section', function() {
      return settings_page_test_util.getPage('about').then(function(page) {
        expectTrue(!!settings_page_test_util.getSection(page, 'about'));
      });
    });
  });

  // Run all registered tests.
  mocha.run();
});
