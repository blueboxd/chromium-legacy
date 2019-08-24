// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<app-management-router>', () => {
  let store;
  let router;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    store = replaceStore();
    await fakeHandler.addApp('1');
    router = document.createElement('app-management-router');
    replaceBody(router);
  });

  test('Search updates from route', async () => {
    await navigateTo('/?q=beep');
    const expected = app_management.actions.setSearchTerm('beep');
    assertDeepEquals(expected, store.lastAction);
  });

  test('Selected app updates from route', async () => {
    await navigateTo('/detail?id=1');
    const expected = app_management.actions.changePage(PageType.DETAIL, '1');

    assertDeepEquals(expected, store.lastAction);
  });

  test('Notifications view appears from route', async () => {
    await navigateTo('/notifications');
    const expected = app_management.actions.changePage(PageType.NOTIFICATIONS);
    assertDeepEquals(expected, store.lastAction);
  });

  test('Route updates from state change', async () => {
    // The application needs an initial url to start with.
    await navigateTo('/');

    store.data.currentPage = {
      pageType: PageType.DETAIL,
      selectedAppId: '1',
    };
    store.notifyObservers();

    await test_util.flushTasks();
    expectEquals('/detail?id=1', getCurrentUrlSuffix());

    // Returning main page clears the route.
    store.data.currentPage = {
      pageType: PageType.MAIN,
      selectedAppId: null,
    };
    store.notifyObservers();
    await test_util.flushTasks();
    expectEquals('/', getCurrentUrlSuffix());

    store.data.currentPage = {
      pageType: PageType.NOTIFICATIONS,
      selectedAppId: null,
    };
    store.notifyObservers();
    await test_util.flushTasks();
    expectEquals('/notifications', getCurrentUrlSuffix());
  });

  test('Route updates from home to search', async () => {
    await navigateTo('/');
    store.data.search = {term: 'bloop'};
    store.notifyObservers();
    await test_util.flushTasks();

    expectEquals('/?q=bloop', getCurrentUrlSuffix());
  });

  test('Route updates from detail to search', async () => {
    await navigateTo('/detail?id=1');
    store.data.search = {term: 'bloop'};
    store.notifyObservers();
    await test_util.flushTasks();

    expectEquals('/detail?id=1&q=bloop', getCurrentUrlSuffix());
  });
});
