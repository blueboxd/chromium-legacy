// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {Route, Router} from 'chrome://settings/settings.js';
// #import {setupPopstateListener} from 'chrome://test/settings/test_util.m.js';
// clang-format on

suite('settings-animated-pages', function() {
  test('focuses subpage trigger when exiting subpage', function(done) {
    const testRoutes = {
      BASIC: new settings.Route('/'),
    };
    testRoutes.SEARCH = testRoutes.BASIC.createSection('/search', 'search');
    testRoutes.SEARCH_ENGINES = testRoutes.SEARCH.createChild('/searchEngines');
    settings.Router.resetInstanceForTesting(new settings.Router(testRoutes));
    test_util.setupPopstateListener();

    document.body.innerHTML = `
      <settings-animated-pages
          section="${testRoutes.SEARCH_ENGINES.section}">
        <div route-path="default">
          <button id="subpage-trigger"></button>
        </div>
        <div route-path="${testRoutes.SEARCH_ENGINES.path}">
          <button id="subpage-trigger"></button>
        </div>
      </settings-animated-pages>`;

    const animatedPages =
        document.body.querySelector('settings-animated-pages');
    animatedPages.focusConfig = new Map();
    animatedPages.focusConfig.set(
        testRoutes.SEARCH_ENGINES.path, '#subpage-trigger');

    const trigger = document.body.querySelector('#subpage-trigger');
    assertTrue(!!trigger);
    trigger.addEventListener('focus', function() {
      done();
    });

    // Trigger subpage exit navigation.
    settings.Router.getInstance().navigateTo(testRoutes.BASIC);
    settings.Router.getInstance().navigateTo(testRoutes.SEARCH_ENGINES);
    settings.Router.getInstance().navigateToPreviousRoute();
  });
});
