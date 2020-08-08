// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

// Polymer 2 test list format:
//
// ['ModuleNameTest', 'module.js',
//   [<module.js dependency source list>]
// ]
[
  ['CrPolicyNetworkBehaviorMojo', 'cr_policy_network_behavior_mojo_tests.js',
    ['../../cr_elements/cr_policy_strings.js']
  ],
  ['NetworkConfig', 'network_config_test.js',
    [
      '//ui/webui/resources/js/assert.js',
      '//ui/webui/resources/js/promise_resolver.js',
      '../../fake_chrome_event.js',
      '../../chromeos/networking_private_constants.js',
      '../../chromeos/fake_network_config_mojom.js',
    ]
  ],
  ['PolicyNetworkIndicatorMojo', 'cr_policy_network_indicator_mojo_tests.js',
    [
      '../../cr_elements/cr_policy_strings.js',
    ]
  ],
].forEach(test => registerTest(...test));

function registerTest(testName, module, deps) {
  const className = `CrComponents${testName}Test`;
  this[className] = class extends PolymerTest {
    /** @override */
    get browsePreload() {
      return `chrome://os-settings/test_loader.html?module=settings/chromeos/${module}`;
    }

    /** @override */
    get extraLibraries() {
      return PolymerTest.prototype.extraLibraries.concat(module).concat(deps);
    }
  };

  TEST_F(className, 'All', () => mocha.run());
}
