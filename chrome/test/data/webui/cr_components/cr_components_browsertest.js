// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer components. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/browser_features.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for shared Polymer components.
 * @constructor
 * @extends {PolymerTest}
 */
function CrComponentsBrowserTest() {}

CrComponentsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  get browsePreload() {
    throw 'subclasses should override to load a WebUI page that includes it.';
  },
};

/**
 * @constructor
 * @extends {CrComponentsBrowserTest}
 */
function CrComponentsManagedFootnoteTest() {}

CrComponentsManagedFootnoteTest.prototype = {
  __proto__: CrComponentsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_components/managed_footnote/managed_footnote.html',

  /** @override */
  extraLibraries: CrComponentsBrowserTest.prototype.extraLibraries.concat([
    'managed_footnote_test.js',
  ]),

  /** @override */
  get suiteName() {
    return managed_footnote_test.suiteName;
  }
};

TEST_F('CrComponentsManagedFootnoteTest', 'Hidden', function() {
  runMochaTest(this.suiteName, managed_footnote_test.TestNames.Hidden);
});

TEST_F('CrComponentsManagedFootnoteTest', 'LoadTimeDataBrowser', function() {
  runMochaTest(
      this.suiteName, managed_footnote_test.TestNames.LoadTimeDataBrowser);
});

TEST_F('CrComponentsManagedFootnoteTest', 'Events', function() {
  runMochaTest(this.suiteName, managed_footnote_test.TestNames.Events);
});

GEN('#if defined(OS_CHROMEOS)');

TEST_F('CrComponentsManagedFootnoteTest', 'LoadTimeDataDevice', function() {
  runMochaTest(
      this.suiteName, managed_footnote_test.TestNames.LoadTimeDataDevice);
});

GEN('#endif');

GEN('#if defined(OS_CHROMEOS)');
/**
 * @constructor
 * @extends {CrComponentsBrowserTest}
 */
function CrPolicyNetworkBehaviorMojoTest() {}

CrPolicyNetworkBehaviorMojoTest.prototype = {
  __proto__: CrComponentsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://os-settings/chromeos/internet_page/internet_page.html',

  /** @override */
  extraLibraries: CrComponentsBrowserTest.prototype.extraLibraries.concat([
    '../cr_elements/cr_policy_strings.js',
    'cr_policy_network_behavior_mojo_tests.js',
  ]),
};

TEST_F('CrPolicyNetworkBehaviorMojoTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrComponentsBrowserTest}
 */
function CrComponentsPolicyNetworkIndicatorMojoTest() {}

CrComponentsPolicyNetworkIndicatorMojoTest.prototype = {
  __proto__: CrComponentsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://os-settings/chromeos/internet_page/internet_page.html',

  /** @override */
  extraLibraries: CrComponentsBrowserTest.prototype.extraLibraries.concat([
    '../cr_elements/cr_policy_strings.js',
    'cr_policy_network_indicator_mojo_tests.js',
  ]),
};

TEST_F('CrComponentsPolicyNetworkIndicatorMojoTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrComponentsBrowserTest}
 */
function CrComponentsNetworkConfigTest() {}

CrComponentsNetworkConfigTest.prototype = {
  __proto__: CrComponentsBrowserTest.prototype,

  /** @override */

  browsePreload: 'chrome://internet-config-dialog',

  /** @override */
  extraLibraries: CrComponentsBrowserTest.prototype.extraLibraries.concat([
    '//ui/webui/resources/js/assert.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '../fake_chrome_event.js',
    '../chromeos/networking_private_constants.js',
    '../chromeos/fake_network_config_mojom.js',
    'network_config_test.js',
  ]),
};

TEST_F('CrComponentsNetworkConfigTest', 'All', function() {
  mocha.run();
});

GEN('#endif');

/**
 * @constructor
 * @extends {CrComponentsBrowserTest}
 */
function CrComponentsNearbyOnboardingPageTest() {}

CrComponentsNearbyOnboardingPageTest.prototype = {
  __proto__: CrComponentsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://resources/cr_components/nearby_share/nearby_onboarding_page.html',

  /** @override */
  featureList: {enabled: ['features::kNearbySharing']},

  /** @override */
  extraLibraries: CrComponentsBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'nearby_onboarding_page_test.js',
  ]),
};

TEST_F('CrComponentsNearbyOnboardingPageTest', 'All', function() {
  mocha.run();
});
