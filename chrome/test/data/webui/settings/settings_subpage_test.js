// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_subpage', function() {
  suite('SettingsSubpage', function() {
    setup(function() {
      const routes = {
        BASIC: new settings.Route('/'),
      };
      routes.SEARCH = routes.BASIC.createSection('/search', 'search');
      routes.SEARCH_ENGINES = routes.SEARCH.createChild('/searchEngines');
      routes.PEOPLE = routes.BASIC.createSection('/people', 'people');
      routes.SYNC = routes.PEOPLE.createChild('/syncSetup');
      routes.PRIVACY = routes.BASIC.createSection('/privacy', 'privacy');
      routes.CERTIFICATES = routes.PRIVACY.createChild('/certificates');

      settings.Router.resetInstanceForTesting(new settings.Router(routes));
      settings.routes = routes;
      test_util.setupPopstateListener();

      PolymerTest.clearBody();
    });

    test('clear search (event)', function() {
      const subpage = document.createElement('settings-subpage');
      // Having a searchLabel will create the cr-search-field.
      subpage.searchLabel = 'test';
      document.body.appendChild(subpage);
      Polymer.dom.flush();
      const search = subpage.$$('cr-search-field');
      assertTrue(!!search);
      search.setValue('Hello');
      subpage.fire('clear-subpage-search');
      Polymer.dom.flush();
      assertEquals('', search.getValue());
    });

    test('clear search (click)', async () => {
      const subpage = document.createElement('settings-subpage');
      // Having a searchLabel will create the cr-search-field.
      subpage.searchLabel = 'test';
      document.body.appendChild(subpage);
      Polymer.dom.flush();
      const search = subpage.$$('cr-search-field');
      assertTrue(!!search);
      search.setValue('Hello');
      assertEquals(null, search.root.activeElement);
      search.$.clearSearch.click();
      await test_util.flushTasks();
      assertEquals('', search.getValue());
      assertEquals(search.$.searchInput, search.root.activeElement);
    });

    test('navigates to parent when there is no history', function() {
      // Pretend that we initially started on the CERTIFICATES route.
      window.history.replaceState(
          undefined, '', settings.routes.CERTIFICATES.path);
      settings.Router.getInstance().initializeRouteFromUrl();
      assertEquals(
          settings.routes.CERTIFICATES,
          settings.Router.getInstance().getCurrentRoute());

      const subpage = document.createElement('settings-subpage');
      document.body.appendChild(subpage);

      subpage.$$('cr-icon-button').click();
      assertEquals(
          settings.routes.PRIVACY,
          settings.Router.getInstance().getCurrentRoute());
    });

    test('navigates to any route via window.back()', function(done) {
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      settings.Router.getInstance().navigateTo(settings.routes.SYNC);
      assertEquals(
          settings.routes.SYNC,
          settings.Router.getInstance().getCurrentRoute());

      const subpage = document.createElement('settings-subpage');
      document.body.appendChild(subpage);

      subpage.$$('cr-icon-button').click();

      window.addEventListener('popstate', function(event) {
        assertEquals(
            settings.Router.getInstance().getRoutes().BASIC,
            settings.Router.getInstance().getCurrentRoute());
        done();
      });
    });

    test('updates the title of the document when active', function() {
      const expectedTitle = 'My Subpage Title';
      settings.Router.getInstance().navigateTo(settings.routes.SEARCH);
      const subpage = document.createElement('settings-subpage');
      subpage.setAttribute('route-path', settings.routes.SEARCH_ENGINES.path);
      subpage.setAttribute('page-title', expectedTitle);
      document.body.appendChild(subpage);

      settings.Router.getInstance().navigateTo(settings.routes.SEARCH_ENGINES);
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
});
