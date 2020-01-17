// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI new tab page page. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "services/network/public/cpp/features.h"');

class NewTabPageBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
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
}

// eslint-disable-next-line no-var
var NewTabPageAppTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/app_test.js';
  }
};

TEST_F('NewTabPageAppTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageMostVisitedTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/most_visited_test.js';
  }
};

TEST_F('NewTabPageMostVisitedTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeDialogTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_dialog_test.js';
  }
};

TEST_F('NewTabPageCustomizeDialogTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeThemesTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_themes_test.js';
  }
};

TEST_F('NewTabPageCustomizeThemesTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageThemeIconTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/theme_icon_test.js';
  }
};

TEST_F('NewTabPageThemeIconTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageUtilsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/utils_test.js';
  }
};

TEST_F('NewTabPageUtilsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var NewTabPageCustomizeShortcutsTest = class extends NewTabPageBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://new-tab-page/test_loader.html?module=new_tab_page/customize_shortcuts_test.js';
  }
};

TEST_F('NewTabPageCustomizeShortcutsTest', 'All', function() {
  mocha.run();
});
