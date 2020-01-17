// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('settings-animated-pages', function() {
  test('focuses subpage trigger when exiting subpage', function(done) {
    const whenReady = settings.routes !== undefined ?
        Promise.resolve() :
        PolymerTest.importHtml('chrome://settings/route.html');
    whenReady.then(() => {
      document.body.innerHTML = `
        <settings-animated-pages
            section="${settings.routes.SEARCH_ENGINES.section}">
          <div route-path="default">
            <button id="subpage-trigger"></button>
          </div>
          <div route-path="${settings.routes.SEARCH_ENGINES.path}">
            <button id="subpage-trigger"></button>
          </div>
        </settings-animated-pages>`;

      const animatedPages =
          document.body.querySelector('settings-animated-pages');
      animatedPages.focusConfig = new Map();
      animatedPages.focusConfig.set(
          settings.routes.SEARCH_ENGINES.path, '#subpage-trigger');

      const trigger = document.body.querySelector('#subpage-trigger');
      assertTrue(!!trigger);
      trigger.addEventListener('focus', function() {
        done();
      });

      // Trigger subpage exit navigation.
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      settings.Router.getInstance().navigateTo(settings.routes.SEARCH_ENGINES);
      settings.Router.getInstance().navigateToPreviousRoute();
    });
  });
});
