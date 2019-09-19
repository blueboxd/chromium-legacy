// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "services/network/public/cpp/features.h"');

var TabStripBrowserTest = class extends testing.Test {
  get isAsync() {
    return true;
  }

  get webuiHost() {
    return 'tab-strip';
  }

  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get featureList() {
    return {enabled: ['network::features::kOutOfBlinkCors']};
  }
};

var TabStripTabListTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://test?module=tab_strip/tab_list_test.js';
  }
};

TEST_F('TabStripTabListTest', 'All', function() {
  mocha.run();
});

var TabStripTabTest = class extends TabStripBrowserTest {
  get browsePreload() {
    return 'chrome://test?module=tab_strip/tab_test.js';
  }
};

TEST_F('TabStripTabTest', 'All', function() {
  mocha.run();
});
