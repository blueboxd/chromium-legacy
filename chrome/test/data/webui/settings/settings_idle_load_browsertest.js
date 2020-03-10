// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for settings-idle-load. */


/**
 * @constructor
 * @extends testing.Test
 */
function SettingsIdleLoadBrowserTest() {}

SettingsIdleLoadBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://settings/controls/setting_idle_load.html',

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '../mocha_adapter.js',
    'idle_load_tests.js',
  ],

  /** @override */
  isAsync: true,
};

TEST_F('SettingsIdleLoadBrowserTest', 'All', function() {
  // Run all registered tests.
  mocha.run();
});
