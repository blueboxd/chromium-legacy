// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the Inline login flow ('Add account' flow) on
 * ChromeOS and desktop Chrome.
 */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var InlineLoginBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    // See Reason enum in components/signin/public/base/signin_metrics.h.
    return 'chrome://chrome-signin/test_loader.html?module=inline_login/inline_login_test.js&reason=5';
  }

  get suiteName() {
    return inline_login_test.suiteName;
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

TEST_F('InlineLoginBrowserTest', 'Initialize', function() {
  this.runMochaTest(inline_login_test.TestNames.Initialize);
});

TEST_F('InlineLoginBrowserTest', 'WebUICallbacks', function() {
  this.runMochaTest(inline_login_test.TestNames.WebUICallbacks);
});

TEST_F('InlineLoginBrowserTest', 'AuthExtHostCallbacks', function() {
  this.runMochaTest(inline_login_test.TestNames.AuthExtHostCallbacks);
});

TEST_F('InlineLoginBrowserTest', 'BackButton', function() {
  this.runMochaTest(inline_login_test.TestNames.BackButton);
});
