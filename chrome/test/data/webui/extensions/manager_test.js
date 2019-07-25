// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-sidebar. */
cr.define('extension_manager_tests', function() {
  /** @enum {string} */
  const TestNames = {
    ChangePages: 'change pages',
    ItemListVisibility: 'item list visibility',
    SplitItems: 'split items',
    PageTitleUpdate: 'updates the title based on current route',
    UrlNavigationToDetails: 'url navigation to details',
    UrlNavigationToActivityLogFail:
        'url navigation to activity log without flag set',
    UrlNavigationToActivityLogSuccess:
        'url navigation to activity log with flag set',
  };

  function getDataByName(list, name) {
    return assert(list.find(function(el) {
      return el.name == name;
    }));
  }

  const suiteName = 'ExtensionManagerTest';

  suite(suiteName, function() {
    /** @type {extensions.Manager} */
    let manager;

    /** @param {string} viewElement */
    function assertViewActive(tagName) {
      assertTrue(!!manager.$.viewManager.querySelector(`${tagName}.active`));
    }

    setup(function() {
      manager = document.querySelector('extensions-manager');
      // Wait for the first view to be active before starting tests, if one is
      // not active already. Sometimes, on Mac with native HTML imports
      // disabled, no views are active at this point.
      return manager.$.viewManager.querySelector('.active') ?
          Promise.resolve() :
          test_util.eventToPromise('view-enter-start', manager);
    });

    test(assert(TestNames.ItemListVisibility), function() {
      const extension = getDataByName(manager.extensions_, 'My extension 1');

      const list = manager.$['items-list'];
      const listHasItemWithName = (name) =>
          !!list.extensions.find(el => el.name == name);

      expectEquals(manager.extensions_, manager.$['items-list'].extensions);
      expectTrue(listHasItemWithName('My extension 1'));

      manager.removeItem_(extension.id);
      Polymer.dom.flush();
      expectFalse(listHasItemWithName('My extension 1'));

      manager.addItem_('extensions_', extension);
      Polymer.dom.flush();
      expectTrue(listHasItemWithName('My extension 1'));
    });

    test(assert(TestNames.SplitItems), function() {
      const sectionHasItemWithName = function(section, name) {
        return !!manager[section].find(function(el) {
          return el.name == name;
        });
      };

      // Test that we properly split up the items into two sections.
      expectTrue(sectionHasItemWithName('extensions_', 'My extension 1'));
      expectTrue(sectionHasItemWithName(
          'apps_', 'Platform App Test: minimal platform app'));
      expectTrue(sectionHasItemWithName('apps_', 'hosted_app'));
      expectTrue(sectionHasItemWithName('apps_', 'Packaged App Test'));
    });

    test(assert(TestNames.ChangePages), function() {
      MockInteractions.tap(
          manager.$$('extensions-toolbar').$$('cr-toolbar').$$('#menuButton'));
      Polymer.dom.flush();

      // We start on the item list.
      MockInteractions.tap(manager.$$('#sidebar').$['sections-extensions']);
      Polymer.dom.flush();
      assertViewActive('extensions-item-list');

      // Switch: item list -> keyboard shortcuts.
      MockInteractions.tap(manager.$$('#sidebar').$['sections-shortcuts']);
      Polymer.dom.flush();
      assertViewActive('extensions-keyboard-shortcuts');

      // Switch: item list -> detail view.
      const item = manager.$['items-list'].$$('extensions-item');
      assert(item);
      item.onDetailsTap_();
      Polymer.dom.flush();
      assertViewActive('extensions-detail-view');

      // Switch: detail view -> keyboard shortcuts.
      MockInteractions.tap(manager.$$('#sidebar').$['sections-shortcuts']);
      Polymer.dom.flush();
      assertViewActive('extensions-keyboard-shortcuts');

      // We get back on the item list.
      MockInteractions.tap(manager.$$('#sidebar').$['sections-extensions']);
      Polymer.dom.flush();
      assertViewActive('extensions-item-list');
    });

    test(assert(TestNames.PageTitleUpdate), function() {
      expectEquals('Extensions', document.title);

      // Open details view with a valid ID.
      extensions.navigation.navigateTo({
        page: extensions.Page.DETAILS,
        extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'
      });
      Polymer.dom.flush();
      expectEquals('Extensions - My extension 1', document.title);

      // Navigate back to the list view and check the page title.
      extensions.navigation.navigateTo({page: extensions.Page.LIST});
      Polymer.dom.flush();
      expectEquals('Extensions', document.title);
    });

    test(assert(TestNames.UrlNavigationToDetails), function() {
      assertViewActive('extensions-detail-view');
      const detailsView = manager.$$('extensions-detail-view');
      expectEquals('ldnnhddmnhbkjipkidpdiheffobcpfmf', detailsView.data.id);

      // Try to open detail view for invalid ID.
      extensions.navigation.navigateTo(
          {page: extensions.Page.DETAILS, extensionId: 'z'.repeat(32)});
      Polymer.dom.flush();
      // Should be re-routed to the main page.
      assertViewActive('extensions-item-list');

      // Try to open detail view with a valid ID.
      extensions.navigation.navigateTo({
        page: extensions.Page.DETAILS,
        extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'
      });
      Polymer.dom.flush();
      assertViewActive('extensions-detail-view');
    });

    test(assert(TestNames.UrlNavigationToActivityLogFail), function() {
      expectFalse(manager.showActivityLog);

      // Try to open activity log with a valid ID.
      extensions.navigation.navigateTo({
        page: extensions.Page.ACTIVITY_LOG,
        extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'
      });
      Polymer.dom.flush();

      // Should be re-routed to details page with showActivityLog set to false.
      assertViewActive('extensions-detail-view');
      const detailsView = manager.$$('extensions-detail-view');
      expectFalse(detailsView.showActivityLog);

      // Try to open activity log with an invalid ID.
      extensions.navigation.navigateTo(
          {page: extensions.Page.ACTIVITY_LOG, extensionId: 'z'.repeat(32)});
      Polymer.dom.flush();
      // Should be re-routed to the main page.
      assertViewActive('extensions-item-list');
    });

    test(assert(TestNames.UrlNavigationToActivityLogSuccess), function() {
      expectTrue(manager.showActivityLog);

      // Try to open activity log with a valid ID.
      extensions.navigation.navigateTo({
        page: extensions.Page.ACTIVITY_LOG,
        extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'
      });
      Polymer.dom.flush();

      // Should be on activity log page.
      assertViewActive('extensions-activity-log');

      // Try to open activity log with an invalid ID.
      extensions.navigation.navigateTo(
          {page: extensions.Page.ACTIVITY_LOG, extensionId: 'z'.repeat(32)});
      Polymer.dom.flush();
      // Should also be on activity log page. See |changePage_| in manager.js
      // for the use case.
      assertViewActive('extensions-activity-log');
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
