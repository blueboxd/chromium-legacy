// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-managed-apps>', () => {
  let appDetailView;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Create a Web app which is installed and pinned by policy
    // and has location set to on and camera set to off by policy.
    const permissionOptions = {};
    permissionOptions[PwaPermissionType.CONTENT_SETTINGS_TYPE_GEOLOCATION] = {
      permissionValue: TriState.kAllow,
      isManaged: true,
    };
    permissionOptions[PwaPermissionType
                          .CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA] = {
      permissionValue: TriState.kBlock,
      isManaged: true
    };
    const policyAppOptions = {
      type: apps.mojom.AppType.kWeb,
      isPinned: apps.mojom.OptionalBool.kTrue,
      isPolicyPinned: apps.mojom.OptionalBool.kTrue,
      installSource: apps.mojom.InstallSource.kPolicy,
      permissions: app_management.FakePageHandler.createWebPermissions(
          permissionOptions),
    };
    const app = await fakeHandler.addApp(null, policyAppOptions);
    // Select created app.
    app_management.Store.getInstance().dispatch(
        app_management.actions.changePage(PageType.DETAIL, app.id));
    appDetailView =
        document.createElement('app-management-pwa-permission-view');
    replaceBody(appDetailView);
    await PolymerTest.flushTasks();
  });

  test('Uninstall button affected by policy', () => {
    const uninstallWrapper =
        appDetailView.$$('app-management-permission-view-header')
            .$$('#uninstall-wrapper');
    expectTrue(!!uninstallWrapper.querySelector('#policy-indicator'));
  });

  test('Permission toggles affected by policy', () => {
    function checkToggle(permissionType, policyAffected) {
      const permissionToggle =
          getPermissionToggleByType(appDetailView, permissionType);
      expectTrue(permissionToggle.$$('cr-toggle').disabled === policyAffected);
      expectTrue(!!permissionToggle.$$('#policy-indicator') === policyAffected);
    }
    checkToggle('CONTENT_SETTINGS_TYPE_NOTIFICATIONS', false);
    checkToggle('CONTENT_SETTINGS_TYPE_GEOLOCATION', true);
    checkToggle('CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA', true);
    checkToggle('CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC', false);
  });

  test('Pin to shelf toggle effected by policy', () => {
    const pinToShelfSetting = appDetailView.$$('#pin-to-shelf-setting')
                                  .$$('app-management-toggle-row');
    expectTrue(!!pinToShelfSetting.$$('#policy-indicator'));
    expectTrue(pinToShelfSetting.$$('cr-toggle').disabled);
  });
});
