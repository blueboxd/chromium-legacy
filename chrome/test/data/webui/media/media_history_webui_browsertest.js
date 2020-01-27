// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Media History WebUI.
 */

GEN('#include "build/build_config.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "media/base/media_switches.h"');

function MediaHistoryWebUIBrowserTest() {}

MediaHistoryWebUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://media-history',

  featureList: {enabled: ['media::kUseMediaHistoryStore']},

  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

// https://crbug.com/1045500: Flaky on Windows.
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('MediaHistoryWebUIBrowserTest', 'MAYBE_All', function() {
  suiteSetup(function() {
    return whenPageIsPopulatedForTest();
  });

  test('check stats table is loaded', function() {
    let statsRows =
        Array.from(document.getElementById('stats-table-body').children);
    assertEquals(5, statsRows.length);

    assertDeepEquals(
        [
          ['mediaEngagement', '0'], ['meta', '3'], ['origin', '0'],
          ['playback', '0'], ['playbackSession', '0']
        ],
        statsRows.map(
            x => [x.children[0].textContent, x.children[1].textContent]));
  });

  mocha.run();
});
