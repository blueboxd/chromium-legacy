// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('AppManagementPageTests', () => {
  let appManagementPage;
  let fakeHandler;
  let store;

  /** @return {Element} */
  function getAppList() {
    return appManagementPage.$$('app-management-main-view').$['app-list'];
  }

  /** @return {number} */
  function getAppListChildren() {
    return getAppList().childElementCount -
        1;  // Ignore the dom-repeat element.
  }

  setup(async () => {
    fakeHandler = setupFakeHandler();
    store = replaceStore();

    appManagementPage = document.createElement('settings-app-management-page');
    assertTrue(!!appManagementPage);
    replaceBody(appManagementPage);

    // TODO: enable when dom-switch is added.
    // await appManagementPage.$$('app-management-dom-switch')
    // .firstRenderForTesting_.promise;
    // await test_util.flushTasks();
  });

  test('loads', async () => {
    // Check that the browser responds to the getApps() message.
    const {apps: initialApps} =
        await app_management.BrowserProxy.getInstance().handler.getApps();
  });

  test('App list renders on page change', () => {
    const appList = getAppList();
    let numApps = 0;

    fakeHandler.addApp()
        .then(() => {
          numApps = 1;
          expectEquals(numApps, getAppListChildren());

          // TODO(jshikaram): Re-enable once dom-switch works.
          // // Click app to go to detail page.
          // appList.querySelector('app-management-app-item').click();
          // return test_util.flushTasks();
        })
        .then(() => {
          return fakeHandler.addApp();
        })
        .then(() => {
          numApps++;

          // TODO(jshikaram): Re-enable once dom-switch works.
          // // Click back button to go to main page.
          // app.$$('app-management-pwa-permission-view')
          //     .$$('app-management-permission-view-header')
          //     .$$('#backButton')
          //     .click();

          test_util.flushTasks();
          expectEquals(numApps, getAppListChildren());
        });
  });
});
