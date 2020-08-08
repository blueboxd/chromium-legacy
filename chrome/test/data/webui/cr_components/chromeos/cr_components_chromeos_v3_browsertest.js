// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 cr_components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "content/public/test/browser_test.h"');

[['CrPolicyNetworkBehaviorMojo', 'cr_policy_network_behavior_mojo_tests.m.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `CrComponents${testName}TestV3`;
  this[className] = class extends PolymerTest {
    /** @override */
    get browsePreload() {
      return `chrome://test?module=cr_components/chromeos/${module}`;
    }

    /** @override */
    get extraLibraries() {
      return [
        '//third_party/mocha/mocha.js',
        '//chrome/test/data/webui/mocha_adapter.js',
      ];
    }

    /** @override */
    get webuiHost() {
      return 'dummyurl';
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}
