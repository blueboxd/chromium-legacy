// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the bookmarks page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/webui/bookmarks/bookmarks_browsertest.h"');

const BookmarksBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks';
  }

  /** @override */
  get typedefCppFixture() {
    return 'BookmarksBrowserTest';
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '../test_store.js',
      'test_command_manager.js',
      'test_store.js',
      'test_timer_proxy.js',
      'test_util.js',
    ];
  }
};

// eslint-disable-next-line no-var
var BookmarksActionsTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'actions_test.js',
    ]);
  }
};

TEST_F('BookmarksActionsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksAppTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'app_test.js',
      '//ui/webui/resources/js/util.js',
    ]);
  }
};

TEST_F('BookmarksAppTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksCommandManagerTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'command_manager_test.js',
    ]);
  }
};

// https://crbug.com/1010381: Flaky.
TEST_F('BookmarksCommandManagerTest', 'DISABLED_AllBCM', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksDNDManagerTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'dnd_manager_test.js',
    ]);
  }
};

// http://crbug.com/803570 : Flaky on Win 7 (dbg)
GEN('#if defined(OS_WIN) && !defined(NDEBUG)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('BookmarksDNDManagerTest', 'MAYBE_All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksEditDialogTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'edit_dialog_test.js',
    ]);
  }
};

TEST_F('BookmarksEditDialogTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksItemTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'item_test.js',
    ]);
  }
};

TEST_F('BookmarksItemTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksListTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'list_test.js',
    ]);
  }
};

TEST_F('BookmarksListTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksReducersTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'reducers_test.js',
    ]);
  }
};

TEST_F('BookmarksReducersTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksRouterTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'router_test.js',
    ]);
  }
};

TEST_F('BookmarksRouterTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksFolderNodeTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'folder_node_test.js',
    ]);
  }
};

TEST_F('BookmarksFolderNodeTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksPolicyTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_browser_proxy.js',
      'test_browser_proxy.js',
      'policy_test.js',
      '//ui/webui/resources/js/cr.js',
    ]);
  }
};

TEST_F('BookmarksPolicyTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksStoreTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'store_test.js',
    ]);
  }
};

TEST_F('BookmarksStoreTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksToolbarTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'toolbar_test.js',
    ]);
  }
};

TEST_F('BookmarksToolbarTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksUtilTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'util_test.js',
    ]);
  }
};

TEST_F('BookmarksUtilTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksExtensionAPITest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_api_test.js',
    ]);
  }

  /** @override */
  testGenPreamble() {
    GEN('SetupExtensionAPITest();');
  }
};

TEST_F('BookmarksExtensionAPITest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksExtensionAPIEditDisabledTest = class extends BookmarksBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'extension_api_test_edit_disabled.js',
    ]);
  }

  /** @override */
  testGenPreamble() {
    GEN('SetupExtensionAPIEditDisabledTest();');
  }
};

TEST_F('BookmarksExtensionAPIEditDisabledTest', 'All', function() {
  mocha.run();
});
