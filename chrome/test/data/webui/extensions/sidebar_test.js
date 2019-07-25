// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_sidebar_tests', function() {
  /** @enum {string} */
  const TestNames = {
    LayoutAndClickHandlers: 'layout and click handlers',
    SetSelected: 'set selected',
  };

  const suiteName = 'ExtensionSidebarTest';

  suite(suiteName, function() {
    /** @type {extensions.Sidebar} */
    let sidebar;

    setup(function() {
      PolymerTest.clearBody();
      sidebar = new extensions.Sidebar();
      document.body.appendChild(sidebar);
    });

    test(assert(TestNames.SetSelected), function() {
      const selector = '.section-item.iron-selected';
      expectFalse(!!sidebar.$$(selector));

      window.history.replaceState(undefined, '', '/shortcuts');
      PolymerTest.clearBody();
      sidebar = new extensions.Sidebar();
      document.body.appendChild(sidebar);
      const whenSelected =
          test_util.eventToPromise('iron-select', sidebar.$.sectionMenu);
      Polymer.dom.flush();
      return whenSelected
          .then(function() {
            expectEquals(sidebar.$$(selector).id, 'sections-shortcuts');

            window.history.replaceState(undefined, '', '/');
            PolymerTest.clearBody();
            sidebar = new extensions.Sidebar();
            document.body.appendChild(sidebar);
            const whenSelected =
                test_util.eventToPromise('iron-select', sidebar.$.sectionMenu);
            Polymer.dom.flush();
            return whenSelected;
          })
          .then(function() {
            expectEquals(sidebar.$$(selector).id, 'sections-extensions');
          });
    });

    test(assert(TestNames.LayoutAndClickHandlers), function(done) {
      extension_test_util.testIcons(sidebar);

      const testVisible = extension_test_util.testVisible.bind(null, sidebar);
      testVisible('#sections-extensions', true);
      testVisible('#sections-shortcuts', true);
      testVisible('#more-extensions', true);

      sidebar.isSupervised = true;
      Polymer.dom.flush();
      testVisible('#more-extensions', false);

      let currentPage;
      extensions.navigation.addListener(newPage => {
        currentPage = newPage;
      });

      MockInteractions.tap(sidebar.$$('#sections-shortcuts'));
      expectDeepEquals(currentPage, {page: extensions.Page.SHORTCUTS});

      MockInteractions.tap(sidebar.$$('#sections-extensions'));
      expectDeepEquals(currentPage, {page: extensions.Page.LIST});

      // Clicking on the link for the current page should close the dialog.
      sidebar.addEventListener('close-drawer', () => done());
      MockInteractions.tap(sidebar.$$('#sections-extensions'));
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
