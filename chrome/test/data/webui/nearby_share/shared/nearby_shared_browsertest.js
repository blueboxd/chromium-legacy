// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer v2 Nearby Share shared tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/browser_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

/**
 * @constructor
 * @extends {PolymerTest.prototype}
 */
function NearbyOnboardingPageTest() {}

NearbyOnboardingPageTest.prototype = {
  __proto__: PolymerTest.prototype,

  /**
   * This test requires a polymer 2 host that can load
   * chrome://nearby or this will fail. This test will be disabled on non-cros
   * hosts for now.
   * @override
   */
  browsePreload: 'chrome://os-settings',

  /** @override */
  featureList: {
    enabled: [
      'features::kNearbySharing',
      // required for linux-blink-cors-rel builder (post CQ)
      'network::features::kOutOfBlinkCors',
    ]
  },

  /** @override */
  extraLibraries: PolymerTest.prototype.extraLibraries.concat([
    '../../test_util.js',
    'fake_nearby_share_settings.js',
    'nearby_onboarding_page_test.js',
  ]),
};

TEST_F('NearbyOnboardingPageTest', 'All', () => mocha.run());
