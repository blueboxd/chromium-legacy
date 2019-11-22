// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for History which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

const HistoryFocusTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://history';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'test_util.js',
    ]);
  }

  /** @override */
  setUp() {
    super.setUp();

    suiteSetup(function() {
      // Wait for the top-level app element to be upgraded.
      return waitForAppUpgrade()
          .then(() => history.ensureLazyLoaded())
          .then(() => {
            $('history-app').queryState_.queryingDisabled = true;
          });
    });
  }
};

// eslint-disable-next-line no-var
var HistoryToolbarFocusTest = class extends HistoryFocusTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'history_toolbar_focus_test.js',
    ]);
  }
};

TEST_F('HistoryToolbarFocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistoryListFocusTest = class extends HistoryFocusTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'history_list_focus_test.js',
    ]);
  }
};

TEST_F('HistoryListFocusTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var HistorySyncedDeviceManagerFocusTest = class extends HistoryFocusTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'history_synced_device_manager_focus_test.js',
    ]);
  }
};

TEST_F('HistorySyncedDeviceManagerFocusTest', 'All', function() {
  mocha.run();
});
