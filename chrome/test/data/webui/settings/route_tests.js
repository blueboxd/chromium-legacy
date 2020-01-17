// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('route', function() {
  /**
   * Returns a new promise that resolves after a window 'popstate' event.
   * @return {!Promise}
   */
  function whenPopState(causeEvent) {
    const promise = new Promise(function(resolve) {
      window.addEventListener('popstate', function callback() {
        window.removeEventListener('popstate', callback);
        resolve();
      });
    });

    causeEvent();
    return promise;
  }

  teardown(function() {
    PolymerTest.clearBody();
  });

  /**
   * Tests a specific navigation situation.
   * @param {!settings.Route} previousRoute
   * @param {!settings.Route} currentRoute
   * @param {!settings.Route} expectedNavigatePreviousResult
   * @return {!Promise}
   */
  function testNavigateBackUsesHistory(
      previousRoute, currentRoute, expectedNavigatePreviousResult) {
    settings.Router.getInstance().navigateTo(previousRoute);
    settings.Router.getInstance().navigateTo(currentRoute);

    return whenPopState(function() {
             settings.Router.getInstance().navigateToPreviousRoute();
           })
        .then(function() {
          assertEquals(
              expectedNavigatePreviousResult,
              settings.Router.getInstance().getCurrentRoute());
        });
  }

  test('tree structure', function() {
    // Set up root page routes.
    const BASIC = new settings.Route('/');
    assertEquals(0, BASIC.depth);

    const ADVANCED = new settings.Route('/advanced');
    assertFalse(ADVANCED.isSubpage());
    assertEquals(0, ADVANCED.depth);

    // Test a section route.
    const PRIVACY = ADVANCED.createSection('/privacy', 'privacy');
    assertEquals(ADVANCED, PRIVACY.parent);
    assertEquals(1, PRIVACY.depth);
    assertFalse(PRIVACY.isSubpage());
    assertFalse(BASIC.contains(PRIVACY));
    assertTrue(ADVANCED.contains(PRIVACY));
    assertTrue(PRIVACY.contains(PRIVACY));
    assertFalse(PRIVACY.contains(ADVANCED));

    // Test a subpage route.
    const SITE_SETTINGS = PRIVACY.createChild('/siteSettings');
    assertEquals('/siteSettings', SITE_SETTINGS.path);
    assertEquals(PRIVACY, SITE_SETTINGS.parent);
    assertEquals(2, SITE_SETTINGS.depth);
    assertFalse(!!SITE_SETTINGS.dialog);
    assertTrue(SITE_SETTINGS.isSubpage());
    assertEquals('privacy', SITE_SETTINGS.section);
    assertFalse(BASIC.contains(SITE_SETTINGS));
    assertTrue(ADVANCED.contains(SITE_SETTINGS));
    assertTrue(PRIVACY.contains(SITE_SETTINGS));

    // Test a sub-subpage route.
    const SITE_SETTINGS_ALL = SITE_SETTINGS.createChild('all');
    assertEquals('/siteSettings/all', SITE_SETTINGS_ALL.path);
    assertEquals(SITE_SETTINGS, SITE_SETTINGS_ALL.parent);
    assertEquals(3, SITE_SETTINGS_ALL.depth);
    assertTrue(SITE_SETTINGS_ALL.isSubpage());

    // Test a non-subpage child of ADVANCED.
    const CLEAR_BROWSER_DATA = ADVANCED.createChild('/clearBrowserData');
    assertFalse(CLEAR_BROWSER_DATA.isSubpage());
    assertEquals('', CLEAR_BROWSER_DATA.section);
  });

  test('no duplicate routes', function() {
    const paths = new Set();
    Object.values(settings.routes).forEach(function(route) {
      assertFalse(paths.has(route.path), route.path);
      paths.add(route.path);
    });
  });

  test('navigate back to parent previous route', function() {
    return testNavigateBackUsesHistory(
        settings.routes.BASIC, settings.routes.PEOPLE, settings.routes.BASIC);
  });

  test('navigate back to non-ancestor shallower route', function() {
    return testNavigateBackUsesHistory(
        settings.routes.ADVANCED, settings.routes.PEOPLE,
        settings.routes.BASIC);
  });

  test('navigate back to sibling route', function() {
    return testNavigateBackUsesHistory(
        settings.routes.APPEARANCE, settings.routes.PEOPLE,
        settings.routes.APPEARANCE);
  });

  test('navigate back to parent when previous route is deeper', function() {
    settings.Router.getInstance().navigateTo(settings.routes.SYNC);
    settings.Router.getInstance().navigateTo(settings.routes.PEOPLE);
    settings.Router.getInstance().navigateToPreviousRoute();
    assertEquals(
        settings.routes.BASIC, settings.Router.getInstance().getCurrentRoute());
  });

  test('navigate back to BASIC when going back from root pages', function() {
    settings.Router.getInstance().navigateTo(settings.routes.PEOPLE);
    settings.Router.getInstance().navigateTo(settings.routes.ADVANCED);
    settings.Router.getInstance().navigateToPreviousRoute();
    assertEquals(
        settings.routes.BASIC, settings.Router.getInstance().getCurrentRoute());
  });

  test('navigateTo respects removeSearch optional parameter', function() {
    const params = new URLSearchParams('search=foo');
    settings.Router.getInstance().navigateTo(settings.routes.BASIC, params);
    assertEquals(
        params.toString(),
        settings.Router.getInstance().getQueryParameters().toString());

    settings.Router.getInstance().navigateTo(
        settings.routes.SITE_SETTINGS, null,
        /* removeSearch */ false);
    assertEquals(
        params.toString(),
        settings.Router.getInstance().getQueryParameters().toString());

    settings.Router.getInstance().navigateTo(
        settings.routes.SEARCH_ENGINES, null,
        /* removeSearch */ true);
    assertEquals(
        '', settings.Router.getInstance().getQueryParameters().toString());
  });

  test('navigateTo ADVANCED forwards to BASIC', function() {
    settings.Router.getInstance().navigateTo(settings.routes.ADVANCED);
    assertEquals(
        settings.routes.BASIC, settings.Router.getInstance().getCurrentRoute());
  });

  test('popstate flag works', function() {
    const router = settings.Router.getInstance();
    router.navigateTo(settings.routes.BASIC);
    assertFalse(router.lastRouteChangeWasPopstate());

    router.navigateTo(settings.routes.PEOPLE);
    assertFalse(router.lastRouteChangeWasPopstate());

    return whenPopState(function() {
             window.history.back();
           })
        .then(function() {
          assertEquals(settings.routes.BASIC, router.getCurrentRoute());
          assertTrue(router.lastRouteChangeWasPopstate());

          router.navigateTo(settings.routes.ADVANCED);
          assertFalse(router.lastRouteChangeWasPopstate());
        });
  });

  test('getRouteForPath trailing slashes', function() {
    assertEquals(
        settings.routes.BASIC,
        settings.Router.getInstance().getRouteForPath('/'));
    assertEquals(null, settings.Router.getInstance().getRouteForPath('//'));

    // Simple path.
    assertEquals(
        settings.routes.PEOPLE,
        settings.Router.getInstance().getRouteForPath('/people/'));
    assertEquals(
        settings.routes.PEOPLE,
        settings.Router.getInstance().getRouteForPath('/people'));

    // Path with a slash.
    assertEquals(
        settings.routes.SITE_SETTINGS_COOKIES,
        settings.Router.getInstance().getRouteForPath('/content/cookies'));
    assertEquals(
        settings.routes.SITE_SETTINGS_COOKIES,
        settings.Router.getInstance().getRouteForPath('/content/cookies/'));
  });

  test('isNavigableDialog', function() {
    assertTrue(settings.routes.CLEAR_BROWSER_DATA.isNavigableDialog);
    assertTrue(settings.routes.RESET_DIALOG.isNavigableDialog);
    assertTrue(settings.routes.SIGN_OUT.isNavigableDialog);
    assertTrue(settings.routes.TRIGGERED_RESET_DIALOG.isNavigableDialog);
    if (!cr.isChromeOS) {
      assertTrue(settings.routes.IMPORT_DATA.isNavigableDialog);
    }

    assertFalse(settings.routes.PRIVACY.isNavigableDialog);
    assertFalse(settings.routes.DEFAULT_BROWSER.isNavigableDialog);
  });

  test('pageVisibility affects route availability', function() {
    settings.pageVisibility = {
      appearance: false,
      autofill: false,
      defaultBrowser: false,
      onStartup: false,
      reset: false,
    };
    loadTimeData.overrideValues({showOSSettings: false});

    const router = settings.buildRouterForTesting();
    const hasRoute = route => router.getRoutes().hasOwnProperty(route);

    assertTrue(hasRoute('BASIC'));

    assertFalse(hasRoute('APPEARANCE'));
    assertFalse(hasRoute('AUTOFILL'));
    assertFalse(hasRoute('DEFAULT_BROWSER'));
    assertFalse(hasRoute('ON_STARTUP'));
    assertFalse(hasRoute('RESET'));
  });

  test(
      'getAbsolutePath works in direct and within-settings navigation',
      function() {
        settings.Router.getInstance().resetRouteForTesting();
        // Check getting the absolute path while not inside settings returns the
        // correct path.
        window.location.href = 'https://example.com/path/to/page.html';
        assertEquals(
            'chrome://settings/cloudPrinters',
            settings.routes.CLOUD_PRINTERS.getAbsolutePath());

        // Check getting the absolute path while inside settings returns the
        // correct path for the current route and a different route.
        settings.Router.getInstance().navigateTo(settings.routes.DOWNLOADS);
        assertEquals(
            'chrome://settings/downloads',
            settings.Router.getInstance().getCurrentRoute().getAbsolutePath());
        assertEquals(
            'chrome://settings/languages',
            settings.routes.LANGUAGES.getAbsolutePath());
      });
});
