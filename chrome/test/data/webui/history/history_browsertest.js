// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Material Design history page.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "base/command_line.h"');
GEN('#include "chrome/test/data/webui/history_ui_browsertest.h"');
GEN('#include "services/network/public/cpp/features.h"');

const HistoryBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/';
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

// eslint-disable-next-line no-var
var HistoryDrawerTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_drawer_test.js';
  }
};

TEST_F('HistoryDrawerTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryItemTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_item_test.js';
  }
};

TEST_F('HistoryItemTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryLinkClickTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/link_click_test.js';
  }
};

TEST_F('HistoryLinkClickTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryListTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_list_test.js';
  }
};

// Times out on debug builders because the History page can take several seconds
// to load in a Debug build. See https://crbug.com/669227.
//GEN('#if !defined(NDEBUG)');
//GEN('#define MAYBE_All DISABLED_All');
//GEN('#else');
GEN('#define MAYBE_All All');
//GEN('#endif');

TEST_F('HistoryListTest', 'MAYBE_All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryMetricsTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_metrics_test.js';
  }
};

TEST_F('HistoryMetricsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryOverflowMenuTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_overflow_menu_test.js';
  }
};

TEST_F('HistoryOverflowMenuTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryRoutingTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_routing_test.js';
  }
};

TEST_F('HistoryRoutingTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryRoutingWithQueryParamTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_routing_with_query_param_test.js';
  }
};

TEST_F('HistoryRoutingWithQueryParamTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistorySyncedTabsTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_synced_tabs_test.js';
  }
};

TEST_F('HistorySyncedTabsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistorySupervisedUserTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_supervised_user_test.js';
  }

  get typedefCppFixture() {
    return 'HistoryUIBrowserTest';
  }

  /** @override */
  testGenPreamble() {
    GEN('  SetDeleteAllowed(false);');
  }
};

GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_AllSupervised DISABLED_All');
GEN('#else');
GEN('#define MAYBE_AllSupervised All');
GEN('#endif');

TEST_F('HistorySupervisedUserTest', 'MAYBE_AllSupervised', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryToolbarTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/history_toolbar_test.js';
  }
};

TEST_F('HistoryToolbarTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistorySearchedLabelTest = class extends HistoryBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://history/test_loader.html?module=history/searched_label_test.js';
  }
};

TEST_F('HistorySearchedLabelTest', 'All', function() {
  mocha.run();
});
