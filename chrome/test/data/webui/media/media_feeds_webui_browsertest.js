// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Media Feeds WebUI.
 */

GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "media/base/media_switches.h"');

function MediaFeedsWebUIBrowserTest() {}

MediaFeedsWebUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://media-feeds',

  featureList: {enabled: ['media::kMediaFeeds']},

  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('MediaFeedsWebUIBrowserTest', 'All', function() {
  suiteSetup(function() {
    return whenPageIsPopulatedForTest();
  });

  test('check feeds table is loaded', function() {
    let feedHeaders =
        Array.from(document.querySelector('#feed-table-header').children);

    assertDeepEquals(
        [
          'ID', 'Url', 'Display Name', 'Last Discovery Time', 'Last Fetch Time',
          'User Status', 'Last Fetch Result', 'Fetch Failed Count',
          'Cache Expiry Time', 'Last Fetch Item Count',
          'Last Fetch Play Next Count', 'Last Fetch Content Types', 'Logos'
        ],
        feedHeaders.map(x => x.textContent.trim()));
  });

  mocha.run();
});
