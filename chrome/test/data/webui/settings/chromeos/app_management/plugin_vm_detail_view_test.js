// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-plugin-vm-detail-view>', function() {
  let pluginVmDetailView;
  let fakeHandler;

  function getPermissionBoolByType(permissionType) {
    return app_management.util.getPermissionValueBool(
        pluginVmDetailView.app_, permissionType);
  }

  async function clickToggle(permissionType) {
    getPermissionToggleByType(pluginVmDetailView, permissionType).click();
    await fakeHandler.flushPipesForTesting();
  }

  function getSelectedAppFromStore() {
    const storeData = app_management.Store.getInstance().data;
    return storeData.apps[storeData.selectedAppId];
  }

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    const permissions = {};
    const permissionIds = [PluginVmPermissionType.PRINTING];
    for (const permissionId of permissionIds) {
      permissions[permissionId] = app_management.util.createPermission(
          permissionId, PermissionValueType.kBool, Bool.kTrue,
          false /*is_managed*/);
    }

    // Add an app, and make it the currently selected app.
    const options = {
      type: apps.mojom.AppType.kPluginVm,
      permissions: permissions
    };
    const app = await fakeHandler.addApp(null, options);
    app_management.Store.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(app.id));

    pluginVmDetailView =
        document.createElement('app-management-plugin-vm-detail-view');
    replaceBody(pluginVmDetailView);
  });

  test('App is rendered correctly', function() {
    assertEquals(
        app_management.Store.getInstance().data.selectedAppId,
        pluginVmDetailView.app_.id);
  });

  test('Toggle permissions', async function() {
    const checkToggle = async (permissionType) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(pluginVmDetailView, permissionType)
                     .checked);

      // Toggle off.
      await clickToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse(
          getPermissionCrToggleByType(pluginVmDetailView, permissionType)
              .checked);

      // Toggle on.
      await clickToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(getPermissionCrToggleByType(pluginVmDetailView, permissionType)
                     .checked);
    };

    await checkToggle('PRINTING');
  });

  test('Pin to shelf toggle', async function() {
    const pinToShelfItem = pluginVmDetailView.$['pin-to-shelf-setting'];
    const toggle = pinToShelfItem.$['toggle-row'].$.toggle;

    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        app_management.util.convertOptionalBoolToBool(
            getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertTrue(toggle.checked);
    assertEquals(
        toggle.checked,
        app_management.util.convertOptionalBoolToBool(
            getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        app_management.util.convertOptionalBoolToBool(
            getSelectedAppFromStore().isPinned));
  });
});
