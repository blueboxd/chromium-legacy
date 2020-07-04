// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-plugin-vm-detail-view>', function() {
  /** @enum {number} */
  const Permissions = {
    CAMERA: 0,
    MICROPHONE: 1,
  };

  let pluginVmDetailView;
  let fakeHandler;
  /** @type {?TestPluginVmBrowserProxy} */
  let pluginVmBrowserProxy = null;
  let appId;

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

  /**
   * @param {boolean} expectedCameraState
   * @param {!HTMLElement} cameraToggle
   * @param {boolean} expectedMicrophoneState
   * @param {!HTMLElement} microphoneToggle
   */
  function assertToggleStates(
      expectedCameraState, cameraToggle, expectedMicrophoneState,
      microphoneToggle) {
    assertEquals(expectedCameraState, cameraToggle.checked);
    assertEquals(expectedMicrophoneState, microphoneToggle.checked);
  }

  /**
   * @param {boolean} expectedCameraState
   * @param {boolean} expectedMicrophoneState
   */
  function assertPermissions(expectedCameraState, expectedMicrophoneState) {
    assertEquals(
        expectedCameraState,
        pluginVmBrowserProxy.permissions[Permissions.CAMERA]);
    assertEquals(
        expectedMicrophoneState,
        pluginVmBrowserProxy.permissions[Permissions.MICROPHONE]);
  }

  /**
   * @param {boolean} booleanToChange
   * @return {OptionalBool}
   */
  function booleanToOptionalBool(booleanToChange) {
    return booleanToChange ? Bool.kTrue : Bool.kFalse;
  }

  // Syncs the permissions stored in pluginVmBrowserProxy with app management.
  function syncPermissions() {
    const app = app_management.Store.getInstance().data.apps[appId];
    app.permissions[PluginVmPermissionType.CAMERA] =
        app_management.util.createPermission(
            PluginVmPermissionType.CAMERA, PermissionValueType.kBool,
            booleanToOptionalBool(
                pluginVmBrowserProxy.permissions[Permissions.CAMERA]),
            /*is_managed=*/ false);
    app.permissions[PluginVmPermissionType.MICROPHONE] =
        app_management.util.createPermission(
            PluginVmPermissionType.MICROPHONE, PermissionValueType.kBool,
            booleanToOptionalBool(
                pluginVmBrowserProxy.permissions[Permissions.MICROPHONE]),
            /*is_managed=*/ false);
  }

  setup(async function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    settings.PluginVmBrowserProxyImpl.instance_ = pluginVmBrowserProxy;
    fakeHandler = setupFakeHandler();
    replaceStore();

    loadTimeData.overrideValues({
      showPluginVmCameraPermissions: true,
      showPluginVmMicrophonePermissions: true,
    });

    const permissions = {};
    const permissionIds = [
      PluginVmPermissionType.PRINTING,
      PluginVmPermissionType.CAMERA,
      PluginVmPermissionType.MICROPHONE,
    ];
    for (const permissionId of permissionIds) {
      permissions[permissionId] = app_management.util.createPermission(
          permissionId, PermissionValueType.kBool, Bool.kTrue,
          false /*is_managed*/);
    }

    pluginVmBrowserProxy.pluginVmRunning = false;
    pluginVmBrowserProxy.permissions =
        [/*isCameraEnabled=*/ true, /*isMicrophoneEnabled=*/ true];

    // Add an app, and make it the currently selected app.
    const options = {
      type: apps.mojom.AppType.kPluginVm,
      permissions: permissions
    };
    const app = await fakeHandler.addApp(null, options);
    appId = app.id;
    app_management.Store.getInstance().dispatch(
        app_management.actions.updateSelectedAppId(appId));
    syncPermissions();

    pluginVmDetailView =
        document.createElement('app-management-plugin-vm-detail-view');
    replaceBody(pluginVmDetailView);
    await fakeHandler.flushPipesForTesting();
  });

  test('App is rendered correctly', function() {
    assertEquals(
        app_management.Store.getInstance().data.selectedAppId,
        pluginVmDetailView.app_.id);
  });

  test('Toggle permissions handled by app settings', async function() {
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

  test('Toggle permissions handled by Plugin Vm', async function() {
    const camera_toggle = pluginVmDetailView.$$('#camera-permission')
                              .$$('#toggle-row')
                              .$$('#toggle');
    const microphone_toggle = pluginVmDetailView.$$('#microphone-permission')
                                  .$$('#toggle-row')
                                  .$$('#toggle');
    assertToggleStates(
        /*expectedCameraState=*/ true, camera_toggle,
        /*expectedMicrophoneState=*/ true, microphone_toggle);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    assertEquals(
        0,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));

    camera_toggle.click();
    await fakeHandler.flushPipesForTesting();
    assertToggleStates(
        /*expectedCameraState=*/ false, camera_toggle,
        /*expectedMicrophoneState=*/ true, microphone_toggle);
    assertPermissions(
        /*expectedCameraState=*/ false, /*expectedMicrophoneState=*/ true);
    syncPermissions();

    camera_toggle.click();
    await fakeHandler.flushPipesForTesting();
    assertToggleStates(
        /*expectedCameraState=*/ true, camera_toggle,
        /*expectedMicrophoneState=*/ true, microphone_toggle);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    syncPermissions();

    microphone_toggle.click();
    await fakeHandler.flushPipesForTesting();
    assertToggleStates(
        /*expectedCameraState=*/ true, camera_toggle,
        /*expectedMicrophoneState=*/ false, microphone_toggle);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ false);
    syncPermissions();

    microphone_toggle.click();
    await fakeHandler.flushPipesForTesting();
    assertToggleStates(
        /*expectedCameraState=*/ true, camera_toggle,
        /*expectedMicrophoneState=*/ true, microphone_toggle);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);

    assertEquals(
        4,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(4, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
  });

  test('Cancel camera permission dialog', async function() {
    const camera_toggle = pluginVmDetailView.$$('#camera-permission')
                              .$$('#toggle-row')
                              .$$('#toggle');
    assertFalse(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
    assertTrue(camera_toggle.checked);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    assertEquals(
        0,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    pluginVmBrowserProxy.pluginVmRunning = true;

    camera_toggle.click();
    await fakeHandler.flushPipesForTesting();
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    assertTrue(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
    const dialog =
        pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog');
    assertEquals(
        dialog.$$('#dialog-text').innerText,
        loadTimeData.getString('pluginVmPermissionDialogCameraLabel'));
    assertFalse(camera_toggle.checked);
    // Assert that the permission did not change.
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    const dialogClosedPromise = test_util.eventToPromise('close', dialog);
    dialog.$$('#dialogCancelButton').click();
    await Promise.all(
        [dialogClosedPromise, fakeHandler.flushPipesForTesting()]);
    syncPermissions();

    assertEquals(
        1,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    assertTrue(camera_toggle.checked);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    assertFalse(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
  });

  test('Cancel microphone permission dialog', async function() {
    const microphone_toggle = pluginVmDetailView.$$('#microphone-permission')
                                  .$$('#toggle-row')
                                  .$$('#toggle');
    assertFalse(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
    assertTrue(microphone_toggle.checked);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    assertEquals(
        0,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    pluginVmBrowserProxy.pluginVmRunning = true;

    microphone_toggle.click();
    await fakeHandler.flushPipesForTesting();
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    assertTrue(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
    const dialog =
        pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog');
    assertEquals(
        loadTimeData.getString('pluginVmPermissionDialogMicrophoneLabel'),
        dialog.$$('#dialog-text').innerText);
    assertFalse(microphone_toggle.checked);
    // Assert that the permission did not change.
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    const dialogClosedPromise = test_util.eventToPromise('close', dialog);
    dialog.$$('#dialogCancelButton').click();
    await Promise.all(
        [dialogClosedPromise, fakeHandler.flushPipesForTesting()]);
    syncPermissions();

    assertEquals(
        1,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    assertTrue(microphone_toggle.checked);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    assertFalse(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
  });

  test('Relaunch camera permission dialog', async function() {
    const camera_toggle = pluginVmDetailView.$$('#camera-permission')
                              .$$('#toggle-row')
                              .$$('#toggle');
    assertFalse(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
    assertTrue(camera_toggle.checked);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    assertEquals(
        0,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('relaunchPluginVm'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    pluginVmBrowserProxy.pluginVmRunning = true;

    camera_toggle.click();
    await fakeHandler.flushPipesForTesting();
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    assertTrue(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
    const dialog =
        pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog');
    assertEquals(
        dialog.$$('#dialog-text').innerText,
        loadTimeData.getString('pluginVmPermissionDialogCameraLabel'));
    assertFalse(camera_toggle.checked);
    // Assert that the permission did not change.
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    const dialogClosedPromise = test_util.eventToPromise('close', dialog);
    dialog.$$('#dialogRelaunchButton').click();
    await Promise.all(
        [dialogClosedPromise, fakeHandler.flushPipesForTesting()]);
    syncPermissions();

    assertEquals(
        1,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(1, pluginVmBrowserProxy.getCallCount('relaunchPluginVm'));
    assertEquals(1, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    assertFalse(camera_toggle.checked);
    assertPermissions(
        /*expectedCameraState=*/ false, /*expectedMicrophoneState=*/ true);
    assertFalse(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
  });

  test('Relaunch microphone permission dialog', async function() {
    const microphone_toggle = pluginVmDetailView.$$('#microphone-permission')
                                  .$$('#toggle-row')
                                  .$$('#toggle');
    assertFalse(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
    assertTrue(microphone_toggle.checked);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    assertEquals(
        0,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('relaunchPluginVm'));
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    pluginVmBrowserProxy.pluginVmRunning = true;

    microphone_toggle.click();
    await fakeHandler.flushPipesForTesting();
    assertEquals(0, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    assertTrue(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
    const dialog =
        pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog');
    assertEquals(
        loadTimeData.getString('pluginVmPermissionDialogMicrophoneLabel'),
        dialog.$$('#dialog-text').innerText);
    assertFalse(microphone_toggle.checked);
    // Assert that the permission did not change.
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ true);
    const dialogClosedPromise = test_util.eventToPromise('close', dialog);
    dialog.$$('#dialogRelaunchButton').click();
    await Promise.all(
        [dialogClosedPromise, fakeHandler.flushPipesForTesting()]);
    syncPermissions();

    assertEquals(
        1,
        pluginVmBrowserProxy.getCallCount(
            'wouldPermissionChangeRequireRelaunch'));
    assertEquals(1, pluginVmBrowserProxy.getCallCount('relaunchPluginVm'));
    assertEquals(1, pluginVmBrowserProxy.getCallCount('setPluginVmPermission'));
    assertFalse(microphone_toggle.checked);
    assertPermissions(
        /*expectedCameraState=*/ true, /*expectedMicrophoneState=*/ false);
    assertFalse(
        !!pluginVmDetailView.$$('app-management-plugin-vm-permission-dialog'));
  });
});
