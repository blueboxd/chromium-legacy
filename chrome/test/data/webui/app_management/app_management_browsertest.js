// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the App Management page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/common/chrome_features.h"');

function AppManagementBrowserTest() {}

AppManagementBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://app-management',

  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    '../test_util.js',
    '../test_store.js',
    'test_util.js',
    'test_store.js',
  ],

  featureList: {enabled: ['features::kAppManagement']},

  /** override */
  runAccessibilityChecks: true,
};

function AppManagementAppTest() {}

AppManagementAppTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'app_test.js',
  ]),
};

TEST_F('AppManagementAppTest', 'DISABLED_All', function() {
  mocha.run();
});

function AppManagementDomSwitchTest() {}

AppManagementDomSwitchTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'dom_switch_test.js',
  ]),
};

TEST_F('AppManagementDomSwitchTest', 'DISABLED_All', function() {
  mocha.run();
});

function AppManagementMainViewTest() {}

AppManagementMainViewTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'main_view_test.js',
  ]),
};

TEST_F('AppManagementMainViewTest', 'DISABLED_All', function() {
  mocha.run();
});

function AppManagementMetadataViewTest() {}

AppManagementMetadataViewTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'metadata_view_test.js',
  ]),
};

TEST_F('AppManagementMetadataViewTest', 'DISABLED_All', function() {
  mocha.run();
});

function AppManagementReducersTest() {}

AppManagementReducersTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'reducers_test.js',
  ]),
};

TEST_F('AppManagementReducersTest', 'DISABLED_All', function() {
  mocha.run();
});

function AppManagementRouterTest() {}

AppManagementRouterTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'router_test.js',
  ]),
};

TEST_F('AppManagementRouterTest', 'DISABLED_All', function() {
  mocha.run();
});

function AppManagementPwaPermissionViewTest() {}

AppManagementPwaPermissionViewTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'pwa_permission_view_test.js',
  ]),
};

TEST_F('AppManagementPwaPermissionViewTest', 'DISABLED_All', function() {
  mocha.run();
});

function AppManagementArcPermissionViewTest() {}

AppManagementArcPermissionViewTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'arc_permission_view_test.js',
  ]),
};

TEST_F('AppManagementArcPermissionViewTest', 'DISABLED_All', function() {
  mocha.run();
});

function AppManagementManagedAppsTest() {}

AppManagementManagedAppsTest.prototype = {
  __proto__: AppManagementBrowserTest.prototype,

  extraLibraries: AppManagementBrowserTest.prototype.extraLibraries.concat([
    'managed_apps_test.js',
  ]),
};

TEST_F('AppManagementManagedAppsTest', 'DISABLED_All', function() {
  mocha.run();
});
