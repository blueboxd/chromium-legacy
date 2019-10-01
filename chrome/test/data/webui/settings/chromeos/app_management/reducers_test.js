// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('app state', function() {
  let apps;

  setup(function() {
    apps = {
      '1': createApp('1'),
      '2': createApp('2'),
    };
  });

  test('updates when an app is added', function() {
    const newApp = createApp('3', {type: 1, title: 'a'});
    const action = app_management.actions.addApp(newApp);
    apps = app_management.AppState.updateApps(apps, action);

    // Check that apps contains a key for each app id.
    assertTrue(!!apps['1']);
    assertTrue(!!apps['2']);
    assertTrue(!!apps['3']);

    // Check that id corresponds to the right app.
    const app = apps['3'];
    assertEquals('3', app.id);
    assertEquals(1, app.type);
    assertEquals('a', app.title);
  });

  test('updates when an app is changed', function() {
    const changedApp = createApp('2', {type: 1, title: 'a'});
    const action = app_management.actions.changeApp(changedApp);
    apps = app_management.AppState.updateApps(apps, action);

    // Check that app has changed.
    const app = apps['2'];
    assertEquals(1, app.type);
    assertEquals('a', app.title);

    // Check that number of apps hasn't changed.
    assertEquals(Object.keys(apps).length, 2);
  });

  test('updates when an app is removed', function() {
    const action = app_management.actions.removeApp('1');
    apps = app_management.AppState.updateApps(apps, action);

    // Check that app is removed.
    assertFalse(!!apps['1']);

    // Check that other app is unaffected.
    assertTrue(!!apps['2']);
  });
});

suite('current page state', function() {
  let state;

  setup(function() {
    state = app_management.util.createInitialState([
      createApp('1'),
      createApp('2'),
    ]);
  });

  test(
      'returns to main page if an app is removed while in its detail page',
      function() {
        state.currentPage.selectedAppId = '1';
        state.currentPage.pageType = PageType.DETAIL;

        let action = app_management.actions.removeApp('1');
        state = app_management.reduceAction(state, action);

        assertEquals(null, state.currentPage.selectedAppId);
        assertEquals(PageType.MAIN, state.currentPage.pageType);

        // Page doesn't change if a different app is removed.
        state.apps['1'] = createApp('1');
        state.currentPage.selectedAppId = '1';
        state.currentPage.pageType = PageType.DETAIL;

        action = app_management.actions.removeApp('2');
        state = app_management.reduceAction(state, action);

        assertEquals('1', state.currentPage.selectedAppId);
        assertEquals(PageType.DETAIL, state.currentPage.pageType);
      });

  test('current page updates when changing to main page', function() {
    // Returning to main page results in no selected app.
    state.currentPage.selectedAppId = '1';
    state.currentPage.pageType = PageType.DETAIL;

    let action = app_management.actions.changePage(PageType.MAIN);
    state = app_management.reduceAction(state, action);

    assertEquals(null, state.currentPage.selectedAppId);
    assertEquals(PageType.MAIN, state.currentPage.pageType);

    // Id is disregarded when changing to main page.
    action = app_management.actions.changePage(PageType.MAIN, '1');
    state = app_management.reduceAction(state, action);

    assertEquals(null, state.currentPage.selectedAppId);
    assertEquals(PageType.MAIN, state.currentPage.pageType);
  });

  test('current page updates when changing to app detail page', function() {
    // State updates when a valid app detail page is selected.
    let action = app_management.actions.changePage(PageType.DETAIL, '2');
    state = app_management.reduceAction(state, action);

    assertEquals('2', state.currentPage.selectedAppId);
    assertEquals(PageType.DETAIL, state.currentPage.pageType);

    // State returns to main page if invalid app id is given.
    state.currentPage.selectedAppId = '2';
    state.currentPage.pageType = PageType.DETAIL;

    action = app_management.actions.changePage(PageType.DETAIL, '3');
    state = app_management.reduceAction(state, action);

    assertEquals(null, state.currentPage.selectedAppId);
    assertEquals(PageType.MAIN, state.currentPage.pageType);
  });
});
