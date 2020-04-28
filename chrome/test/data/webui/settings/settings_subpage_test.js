// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Route, Router} from 'chrome://settings/settings.js';
import {setupPopstateListener} from 'chrome://test/settings/test_util.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

// clang-format on

suite('SettingsSubpage', function() {
  let testRoutes;

  setup(function() {
    testRoutes = {
      BASIC: new Route('/'),
    };
    testRoutes.SEARCH = testRoutes.BASIC.createSection('/search', 'search');
    testRoutes.SEARCH_ENGINES = testRoutes.SEARCH.createChild('/searchEngines');
    testRoutes.PEOPLE = testRoutes.BASIC.createSection('/people', 'people');
    testRoutes.SYNC = testRoutes.PEOPLE.createChild('/syncSetup');
    testRoutes.PRIVACY = testRoutes.BASIC.createSection('/privacy', 'privacy');
    testRoutes.CERTIFICATES = testRoutes.PRIVACY.createChild('/certificates');

    Router.resetInstanceForTesting(new Router(testRoutes));

    setupPopstateListener();

    PolymerTest.clearBody();
  });

  test('clear search (event)', function() {
    const subpage = document.createElement('settings-subpage');
    // Having a searchLabel will create the cr-search-field.
    subpage.searchLabel = 'test';
    document.body.appendChild(subpage);
    flush();
    const search = subpage.$$('cr-search-field');
    assertTrue(!!search);
    search.setValue('Hello');
    subpage.fire('clear-subpage-search');
    flush();
    assertEquals('', search.getValue());
  });

  test('clear search (click)', async () => {
    const subpage = document.createElement('settings-subpage');
    // Having a searchLabel will create the cr-search-field.
    subpage.searchLabel = 'test';
    document.body.appendChild(subpage);
    flush();
    const search = subpage.$$('cr-search-field');
    assertTrue(!!search);
    search.setValue('Hello');
    assertEquals(null, search.root.activeElement);
    search.$.clearSearch.click();
    await flushTasks();
    assertEquals('', search.getValue());
    assertEquals(search.$.searchInput, search.root.activeElement);
  });

  test('navigates to parent when there is no history', function() {
    // Pretend that we initially started on the CERTIFICATES route.
    window.history.replaceState(undefined, '', testRoutes.CERTIFICATES.path);
    Router.getInstance().initializeRouteFromUrl();
    assertEquals(
        testRoutes.CERTIFICATES, Router.getInstance().getCurrentRoute());

    const subpage = document.createElement('settings-subpage');
    document.body.appendChild(subpage);

    subpage.$$('cr-icon-button').click();
    assertEquals(testRoutes.PRIVACY, Router.getInstance().getCurrentRoute());
  });

  test('navigates to any route via window.back()', function(done) {
    Router.getInstance().navigateTo(testRoutes.BASIC);
    Router.getInstance().navigateTo(testRoutes.SYNC);
    assertEquals(testRoutes.SYNC, Router.getInstance().getCurrentRoute());

    const subpage = document.createElement('settings-subpage');
    document.body.appendChild(subpage);

    subpage.$$('cr-icon-button').click();

    window.addEventListener('popstate', function(event) {
      assertEquals(
          Router.getInstance().getRoutes().BASIC,
          Router.getInstance().getCurrentRoute());
      done();
    });
  });

  test('updates the title of the document when active', function() {
    const expectedTitle = 'My Subpage Title';
    Router.getInstance().navigateTo(testRoutes.SEARCH);
    const subpage = document.createElement('settings-subpage');
    subpage.setAttribute('route-path', testRoutes.SEARCH_ENGINES.path);
    subpage.setAttribute('page-title', expectedTitle);
    document.body.appendChild(subpage);

    Router.getInstance().navigateTo(testRoutes.SEARCH_ENGINES);
    assertEquals(
        document.title,
        loadTimeData.getStringF('settingsAltPageTitle', expectedTitle));
  });
});

suite('SettingsSubpageSearch', function() {
  test('host autofocus propagates to <cr-input>', function() {
    PolymerTest.clearBody();
    const element = document.createElement('cr-search-field');
    element.setAttribute('autofocus', true);
    document.body.appendChild(element);

    assertTrue(element.$$('cr-input').hasAttribute('autofocus'));

    element.removeAttribute('autofocus');
    assertFalse(element.$$('cr-input').hasAttribute('autofocus'));
  });
});
