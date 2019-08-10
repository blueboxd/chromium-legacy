// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer welcome tests on welcome UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/webui/welcome/helpers.h"');

/** Test fixture for Polymer welcome elements. */
const WelcomeBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
  }

  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '../test_browser_proxy.js',
    ];
  }

  /** @override */
  get featureList() {
    return {enabled: ['welcome::kForceEnabled']};
  }
};

// eslint-disable-next-line no-var
var WelcomeAppChooserTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/google_apps/nux_google_apps.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'app_chooser_test.js',
      'test_google_app_proxy.js',
      'test_metrics_proxy.js',
      'test_bookmark_proxy.js',
    ]);
  }
};

TEST_F('WelcomeAppChooserTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeWelcomeAppTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/welcome_app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'welcome_app_test.js',
      'test_bookmark_proxy.js',
      'test_welcome_browser_proxy.js',
      'test_nux_set_as_default_proxy.js',
    ]);
  }
};

TEST_F('WelcomeWelcomeAppTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeSigninViewTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/signin_view.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'signin_view_test.js',
      'test_welcome_browser_proxy.js',
    ]);
  }
};

TEST_F('WelcomeSigninViewTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeNavigationBehaviorTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/navigation_behavior.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'navigation_behavior_test.js',
    ]);
  }
};

TEST_F('WelcomeNavigationBehaviorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeModuleMetricsTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/shared/module_metrics_proxy.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'module_metrics_test.js',
      'test_metrics_proxy.js',
    ]);
  }
};

TEST_F('WelcomeModuleMetricsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeSetAsDefaultTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/set_as_default/nux_set_as_default.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../test_util.js',
      'nux_set_as_default_test.js',
      'test_nux_set_as_default_proxy.js',
    ]);
  }
};

TEST_F('WelcomeSetAsDefaultTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeNtpBackgroundTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/ntp_background/nux_ntp_background.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nux_ntp_background_test.js',
      'test_metrics_proxy.js',
      'test_ntp_background_proxy.js',
    ]);
  }
};

TEST_F('WelcomeNtpBackgroundTest', 'All', function() {
  mocha.run();
});
