// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-pwa-permission-view>', function() {
  let pwaPermissionView;
  let fakeHandler;

  function getPermissionToggleByType(permissionType) {
    return pwaPermissionView.root
        .querySelector('[permission-type=' + permissionType + ']')
        .root.querySelector('app-management-permission-toggle')
        .root.querySelector('cr-toggle');
  }

  function getPermissionBoolByType(permissionType) {
    return app_management.util.getPermissionValueBool(
        pwaPermissionView.app_, permissionType);
  }

  async function clickToggle(permissionType) {
    getPermissionToggleByType(permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Add an app, and make it the currently selected app.
    const app = await fakeHandler.addApp();
    app_management.Store.getInstance().dispatch(
        app_management.actions.changePage(PageType.DETAIL, app.id));

    pwaPermissionView =
        document.createElement('app-management-pwa-permission-view');
    replaceBody(pwaPermissionView);
  });

  test('App is rendered correctly', function() {
    assertEquals(
        app_management.Store.getInstance().data.currentPage.selectedAppId,
        pwaPermissionView.app_.id);
  });

  test('toggle permissions', async function() {
    const checkToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionToggleByType(permissionType).checked);

      // Toggle off.
      await clickToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(getPermissionToggleByType(permissionType).checked);

      // Toggle on.
      await clickToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionToggleByType(permissionType).checked);
    };

    await checkToggle('CONTENT_SETTINGS_TYPE_NOTIFICATIONS');
    await checkToggle('CONTENT_SETTINGS_TYPE_GEOLOCATION');
    await checkToggle('CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA');
    await checkToggle('CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC');
  });
});
