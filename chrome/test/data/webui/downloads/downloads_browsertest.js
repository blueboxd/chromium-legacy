// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the Material Design downloads page. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function DownloadsTest() {}

DownloadsTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },

  /** @override */
  runAccessibilityChecks: true,
};

/**
 * @constructor
 * @extends {DownloadsTest}
 */
function DownloadsItemTest() {}

DownloadsItemTest.prototype = {
  __proto__: DownloadsTest.prototype,

  /** @override */
  browsePreload: 'chrome://downloads/item.html',

  /** @override */
  extraLibraries: DownloadsTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/cr.js',
    '../test_browser_proxy.js',
    'test_support.js',
    'item_tests.js',
  ]),
};

TEST_F('DownloadsItemTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {DownloadsTest}
 */
function DownloadsManagerTest() {}

DownloadsManagerTest.prototype = {
  __proto__: DownloadsTest.prototype,

  /** @override */
  browsePreload: 'chrome://downloads/',

  /** @override */
  extraLibraries: DownloadsTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/cr.js',
    '//chrome/browser/resources/downloads/constants.js',
    '../test_browser_proxy.js',
    'test_support.js',
    'manager_tests.js',
  ]),
};

TEST_F('DownloadsManagerTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {DownloadsTest}
 */
function DownloadsToolbarTest() {}

DownloadsToolbarTest.prototype = {
  __proto__: DownloadsTest.prototype,

  /** @override */
  browsePreload: 'chrome://downloads/toolbar.html',

  /** @override */
  extraLibraries: DownloadsTest.prototype.extraLibraries.concat([
    'toolbar_tests.js',
  ]),
};

TEST_F('DownloadsToolbarTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {DownloadsTest}
 */
function DownloadsUrlTest() {}

DownloadsUrlTest.prototype = {
  __proto__: DownloadsTest.prototype,

  /** @override */
  browsePreload: 'chrome://downloads/a/b/',
};

TEST_F('DownloadsUrlTest', 'All', function() {
  suite('loading a nonexistent URL of /a/b/', function() {
    test('should load main page with no console errors', function() {
      return customElements.whenDefined('downloads-manager').then(() => {
        assertEquals('chrome://downloads/', location.href);
      });
    });
  });
  mocha.run();
});
