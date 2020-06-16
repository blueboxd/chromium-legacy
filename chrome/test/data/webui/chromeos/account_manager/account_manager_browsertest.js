// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs UI tests for account manager dialogs. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function AccountManagerBrowserTest() {}

AccountManagerBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://account-migration-welcome/',

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    '../../test_browser_proxy.js',
    'test_account_manager_browser_proxy.js',
    'account_migration_welcome_test.js',
  ],
};

TEST_F('AccountManagerBrowserTest', 'AccountMigrationWelcomeTests', () => {
  account_manager.account_migration_welcome_tests.registerTests();
  mocha.run();
});
