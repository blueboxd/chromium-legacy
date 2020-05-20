// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Sign-in web UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "base/command_line.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

class SigninBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
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
    return {enabled: ['network::features::kOutOfBlinkCors']};
  }
}

/**
 * Test fixture for
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var SigninSyncConfirmationTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://sync-confirmation/test_loader.html?module=signin/sync_confirmation_test.js';
  }
};

TEST_F('SigninSyncConfirmationTest', 'Dialog', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/signin_reauth/signin_reauth.html.
 */
// eslint-disable-next-line no-var
var SigninReauthTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://signin-reauth/test_loader.html?module=signin/signin_reauth_test.js';
  }
};

TEST_F('SigninReauthTest', 'Dialog', function() {
  mocha.run();
});
