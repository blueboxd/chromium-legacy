// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the bookmarks page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/prefs/incognito_mode_prefs.h"');
GEN('#include "chrome/browser/ui/webui/bookmarks/bookmarks_browsertest.h"');

function BookmarksBrowserTest() {}

BookmarksBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://bookmarks',

  typedefCppFixture: 'BookmarksBrowserTest',

  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    '../test_store.js',
    'test_command_manager.js',
    'test_store.js',
    'test_timer_proxy.js',
    'test_util.js',
  ],

  /** override */
  runAccessibilityChecks: true,
};

function BookmarksActionsTest() {}

BookmarksActionsTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'actions_test.js',
  ]),
};

TEST_F('BookmarksActionsTest', 'All', function() {
  mocha.run();
});

function BookmarksAppTest() {}

BookmarksAppTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'app_test.js',
    '//ui/webui/resources/js/util.js',
  ]),
};

TEST_F('BookmarksAppTest', 'All', function() {
  mocha.run();
});

function BookmarksCommandManagerTest() {}

BookmarksCommandManagerTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'command_manager_test.js',
  ]),
};

TEST_F('BookmarksCommandManagerTest', 'All', function() {
  mocha.run();
});

function BookmarksDNDManagerTest() {}

BookmarksDNDManagerTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'dnd_manager_test.js',
  ]),
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

function BookmarksEditDialogTest() {}

BookmarksEditDialogTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'edit_dialog_test.js',
  ]),
};

TEST_F('BookmarksEditDialogTest', 'All', function() {
  mocha.run();
});

function BookmarksItemTest() {}

BookmarksItemTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'item_test.js',
  ]),
};

TEST_F('BookmarksItemTest', 'All', function() {
  mocha.run();
});

function BookmarksListTest() {}

BookmarksListTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'list_test.js',
  ]),
};

TEST_F('BookmarksListTest', 'All', function() {
  mocha.run();
});

function BookmarksReducersTest() {}

BookmarksReducersTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'reducers_test.js',
  ]),
};

TEST_F('BookmarksReducersTest', 'All', function() {
  mocha.run();
});

function BookmarksRouterTest() {}

BookmarksRouterTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    '../test_util.js',
    'router_test.js',
  ]),
};

TEST_F('BookmarksRouterTest', 'All', function() {
  mocha.run();
});

function BookmarksFolderNodeTest() {}

BookmarksFolderNodeTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'folder_node_test.js',
  ]),
};

TEST_F('BookmarksFolderNodeTest', 'All', function() {
  mocha.run();
});

function BookmarksPolicyTest() {}

BookmarksPolicyTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  testGenPreamble: function() {
    GEN('SetIncognitoAvailability(IncognitoModePrefs::DISABLED);');
    GEN('SetCanEditBookmarks(false);');
  },

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'policy_test.js',
  ]),
};

TEST_F('BookmarksPolicyTest', 'All', function() {
  mocha.run();
});

function BookmarksStoreTest() {}

BookmarksStoreTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'store_test.js',
  ]),
};

TEST_F('BookmarksStoreTest', 'All', function() {
  mocha.run();
});

function BookmarksToolbarTest() {}

BookmarksToolbarTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'toolbar_test.js',
  ]),
};

TEST_F('BookmarksToolbarTest', 'All', function() {
  mocha.run();
});

function BookmarksUtilTest() {}

BookmarksUtilTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'util_test.js',
  ]),
};

TEST_F('BookmarksUtilTest', 'All', function() {
  mocha.run();
});

function BookmarksExtensionAPITest() {}

BookmarksExtensionAPITest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'extension_api_test.js',
  ]),

  testGenPreamble: () => {
    GEN('SetupExtensionAPITest();');
  }
};

TEST_F('BookmarksExtensionAPITest', 'All', function() {
  mocha.run();
});

function BookmarksExtensionAPIEditDisabledTest() {}

BookmarksExtensionAPIEditDisabledTest.prototype = {
  __proto__: BookmarksBrowserTest.prototype,

  extraLibraries: BookmarksBrowserTest.prototype.extraLibraries.concat([
    'extension_api_test_edit_disabled.js',
  ]),

  testGenPreamble: () => {
    GEN('SetupExtensionAPIEditDisabledTest();');
  }
};

TEST_F('BookmarksExtensionAPIEditDisabledTest', 'All', function() {
  mocha.run();
});
