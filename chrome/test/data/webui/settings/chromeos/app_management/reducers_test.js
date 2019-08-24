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

  test('current page updates when changing to notifications page', function() {
    const action = app_management.actions.changePage(PageType.NOTIFICATIONS);
    state = app_management.reduceAction(state, action);

    assertEquals(PageType.NOTIFICATIONS, state.currentPage.pageType);
  });
});

suite('search state', function() {
  let state;

  setup(function() {
    state = app_management.util.createInitialState([
      createApp('1'),
      createApp('2'),
    ]);
  });

  test('state updates when search starts', function() {
    // State updates when a search term has been typed in.
    let action = app_management.actions.setSearchTerm('searchTerm');
    state = app_management.reduceAction(state, action);

    assertEquals('searchTerm', state.search.term);

    // Search disappears when there is no term entered.
    action = app_management.actions.clearSearch();
    state = app_management.reduceAction(state, action);
    assertEquals(PageType.MAIN, state.currentPage.pageType);

    assertEquals(null, state.search.term);
  });
});

suite('notifications state', function() {
  let state;

  function createAppWithNotifications(id, allowed) {
    const permissionValue = allowed ? TriState.kAllow : TriState.kBlock;
    const notificationsPermissionType =
        PwaPermissionType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS;

    const notificationsPermission = app_management.util.createPermission(
        notificationsPermissionType, PermissionValueType.kTriState,
        permissionValue);

    const permissions = {};
    permissions[notificationsPermissionType] = notificationsPermission;
    return createApp(id, {permissions: permissions});
  }

  setup(function() {
    state = app_management.util.createInitialState([
      createAppWithNotifications('1', false),
      createAppWithNotifications('2', true),
    ]);
  });

  test('notifications state updates when an app is added', function() {
    // Check that the sets are initialised correctly.
    let {allowedIds, blockedIds} = state.notifications;

    assertEquals(1, allowedIds.size);
    assertTrue(allowedIds.has('2'));

    assertEquals(1, blockedIds.size);
    assertTrue(blockedIds.has('1'));

    // Add an app and update the sets.
    let newApp = createAppWithNotifications('3', true);
    let action = app_management.actions.addApp(newApp);
    let {allowedIds: newAllowedIds, blockedIds: newBlockedIds} =
        app_management.NotificationsState.updateNotifications(
            {allowedIds, blockedIds}, action);

    // Check that the new id was added to the allowed set, and its reference
    // has changed.
    assertEquals(2, newAllowedIds.size);
    assertTrue(newAllowedIds.has('3'));
    assertNotEquals(newAllowedIds, allowedIds);

    // Check that the blocked set hasn't changed, and that its reference is
    // the same.
    assertEquals(1, newBlockedIds.size);
    assertEquals(newBlockedIds, blockedIds);

    // If the added doesn't have a notifications permission, the sets don't
    // change.
    allowedIds = newAllowedIds;
    blockedIds = newBlockedIds;

    newApp = createApp('4', {type: AppType.kUnknown, permissions: {}});
    action = app_management.actions.addApp(newApp);
    ({allowedIds: newAllowedIds, blockedIds: newBlockedIds} =
         app_management.NotificationsState.updateNotifications(
             {allowedIds, blockedIds}, action));

    assertEquals(2, newAllowedIds.size);
    assertEquals(newAllowedIds, allowedIds);

    assertEquals(1, newBlockedIds.size);
    assertEquals(newBlockedIds, blockedIds);
  });

  test('notifications state updates when an app is changed', function() {
    // If the change doesn't affect the notifications permission, the sets
    // are unchanged and still have the same references.
    let {allowedIds, blockedIds} = state.notifications;

    let changedApp = createAppWithNotifications('2', true);
    changedApp.title = 'New App Title';
    let action = app_management.actions.changeApp(changedApp);
    let {allowedIds: newAllowedIds, blockedIds: newBlockedIds} =
        app_management.NotificationsState.updateNotifications(
            {allowedIds, blockedIds}, action);

    assertEquals(1, newAllowedIds.size);
    assertTrue(newAllowedIds.has('2'));
    assertEquals(newAllowedIds, allowedIds);

    assertEquals(1, newBlockedIds.size);
    assertTrue(newBlockedIds.has('1'));
    assertEquals(newBlockedIds, blockedIds);

    // If the notifications permission value of an app is changed, the sets
    // update correctly.
    blockedIds = newBlockedIds;
    allowedIds = newAllowedIds;

    changedApp = createAppWithNotifications('1', true);
    action = app_management.actions.changeApp(changedApp);
    ({allowedIds: newAllowedIds, blockedIds: newBlockedIds} =
         app_management.NotificationsState.updateNotifications(
             {allowedIds, blockedIds}, action));

    assertEquals(2, newAllowedIds.size);
    assertTrue(newAllowedIds.has('1'));
    assertNotEquals(newAllowedIds, allowedIds);

    assertEquals(0, newBlockedIds.size);
    assertNotEquals(newBlockedIds, blockedIds);
  });

  test('notifications state updates when an app is removed', function() {
    // When an app is removed, the sets update correctly.
    const {allowedIds, blockedIds} = state.notifications;

    const action = app_management.actions.removeApp('1');
    const {allowedIds: newAllowedIds, blockedIds: newBlockedIds} =
        app_management.NotificationsState.updateNotifications(
            {allowedIds, blockedIds}, action);

    assertEquals(1, newAllowedIds.size);
    assertTrue(newAllowedIds.has('2'));
    assertEquals(newAllowedIds, allowedIds);

    assertEquals(0, newBlockedIds.size);
    assertNotEquals(newBlockedIds, blockedIds);
  });
});
