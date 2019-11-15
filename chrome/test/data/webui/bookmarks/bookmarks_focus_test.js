// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for Bookmarks which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

const BookmarksFocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://bookmarks';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '//ui/webui/resources/js/util.js',
      '../test_util.js',
      '../test_store.js',
      'test_command_manager.js',
      'test_store.js',
      'test_util.js',
    ]);
  }
};

// eslint-disable-next-line no-var
var BookmarksFolderNodeFocusTest = class extends BookmarksFocusTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'folder_node_focus_test.js',
    ]);
  }
};

// http://crbug.com/1000950 : Flaky.
GEN('#define MAYBE_All DISABLED_All');
TEST_F('BookmarksFolderNodeFocusTest', 'MAYBE_All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksListFocusTest = class extends BookmarksFocusTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'list_focus_test.js',
    ]);
  }
};

// http://crbug.com/1000950 : Flaky.
GEN('#define MAYBE_All DISABLED_All');
TEST_F('BookmarksListFocusTest', 'MAYBE_All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var BookmarksDialogFocusManagerTest = class extends BookmarksFocusTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'dialog_focus_manager_test.js',
    ]);
  }
};

// http://crbug.com/1000950 : Flaky.
GEN('#define MAYBE_All DISABLED_All');
TEST_F('BookmarksDialogFocusManagerTest', 'MAYBE_All', function() {
  mocha.run();
});
