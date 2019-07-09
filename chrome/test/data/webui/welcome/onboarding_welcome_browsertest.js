// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer welcome tests on onboarding-welcome UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/webui/welcome/nux_helper.h"');

/**
 * Test fixture for Polymer onboarding welcome elements.
 */
const OnboardingWelcomeBrowserTest = class extends PolymerTest {
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
    return {enabled: ['nux::kNuxOnboardingForceEnabled']};
  }
};

// eslint-disable-next-line no-var
var OnboardingWelcomeAppChooserTest =
    class extends OnboardingWelcomeBrowserTest {
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

TEST_F('OnboardingWelcomeAppChooserTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var OnboardingWelcomeWelcomeAppTest =
    class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/welcome_app.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'welcome_app_test.js',
      'test_bookmark_proxy.js',
      'test_welcome_browser_proxy.js',
      'test_nux_set_as_default_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeWelcomeAppTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var OnboardingWelcomeSigninViewTest =
    class extends OnboardingWelcomeBrowserTest {
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

TEST_F('OnboardingWelcomeSigninViewTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var OnboardingWelcomeNavigationBehaviorTest =
    class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/navigation_behavior.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'navigation_behavior_test.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeNavigationBehaviorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var OnboardingWelcomeModuleMetricsTest =
    class extends OnboardingWelcomeBrowserTest {
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

TEST_F('OnboardingWelcomeModuleMetricsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var OnboardingWelcomeSetAsDefaultTest =
    class extends OnboardingWelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/set_as_default/nux_set_as_default.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../settings/test_util.js',
      'nux_set_as_default_test.js',
      'test_nux_set_as_default_proxy.js',
    ]);
  }
};

TEST_F('OnboardingWelcomeSetAsDefaultTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var OnboardingWelcomeNtpBackgroundTest =
    class extends OnboardingWelcomeBrowserTest {
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

TEST_F('OnboardingWelcomeNtpBackgroundTest', 'All', function() {
  mocha.run();
});
