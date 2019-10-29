// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Sign-in web UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "base/command_line.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "chrome/test/data/webui/signin_browsertest.h"');
GEN('#include "services/network/public/cpp/features.h"');

/**
 * Test fixture for
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var SigninSyncConfirmationTest = class extends PolymerTest {
  /** @override */
  get typedefCppFixture() {
    return 'SigninBrowserTest';
  }

  /** @override */
  get browsePreload() {
    return 'chrome://sync-confirmation/test_loader.html?module=signin/sync_confirmation_test.js';
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
};

// TODO(https://crbug.com/862573): Re-enable when no longer failing when
// is_chrome_branded is true.
GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
GEN('#define MAYBE_DialogWithDice DISABLED_DialogWithDice');
GEN('#else');
GEN('#define MAYBE_DialogWithDice');
GEN('#endif');
TEST_F('SigninSyncConfirmationTest', 'MAYBE_DialogWithDice', function() {
  mocha.run();
});
