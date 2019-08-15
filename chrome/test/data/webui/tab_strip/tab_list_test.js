// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab_list.js';
import {TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';
import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('TabList', () => {
  let callbackRouter;
  let optionsCalled;
  let tabList;
  let testTabsApiProxy;

  const currentWindow = {
    id: 1001,
    tabs: [
      {
        active: true,
        id: 0,
        index: 0,
        title: 'Tab 1',
        windowId: 1001,
      },
      {
        active: false,
        id: 1,
        index: 1,
        title: 'Tab 2',
        windowId: 1001,
      },
      {
        active: false,
        id: 2,
        index: 2,
        title: 'Tab 3',
        windowId: 1001,
      },
    ],
  };

  setup(() => {
    document.body.innerHTML = '';

    testTabsApiProxy = new TestTabsApiProxy();
    testTabsApiProxy.setCurrentWindow(currentWindow);
    TabsApiProxy.instance_ = testTabsApiProxy;

    callbackRouter = testTabsApiProxy.callbackRouter;

    tabList = document.createElement('tabstrip-tab-list');
    document.body.appendChild(tabList);
    return testTabsApiProxy.whenCalled('getCurrentWindow');
  });

  test('creates a tab element for each tab', () => {
    const tabElements = tabList.shadowRoot.querySelectorAll('tabstrip-tab');
    assertEquals(currentWindow.tabs.length, tabElements.length);
    currentWindow.tabs.forEach((tab, index) => {
      assertEquals(tabElements[index].tab, tab);
    });
  });

  test('adds a new tab element when a tab is added in same window', () => {
    const appendedTab = {
      id: 3,
      index: 3,
      title: 'New tab',
      windowId: currentWindow.id,
    };
    callbackRouter.onCreated.dispatchEvent(appendedTab);
    let tabElements = tabList.shadowRoot.querySelectorAll('tabstrip-tab');
    assertEquals(currentWindow.tabs.length + 1, tabElements.length);
    assertEquals(tabElements[currentWindow.tabs.length].tab, appendedTab);

    const prependedTab = {
      id: 4,
      index: 0,
      title: 'New tab',
      windowId: currentWindow.id,
    };
    callbackRouter.onCreated.dispatchEvent(prependedTab);
    tabElements = tabList.shadowRoot.querySelectorAll('tabstrip-tab');
    assertEquals(currentWindow.tabs.length + 2, tabElements.length);
    assertEquals(tabElements[0].tab, prependedTab);
  });

  test(
      'does not add a new tab element when a tab is added in a different ' +
          'window',
      () => {
        const newTab = {
          index: 3,
          title: 'New tab',
          windowId: currentWindow.id + 1,
        };
        callbackRouter.onCreated.dispatchEvent(newTab);
        const tabElements = tabList.shadowRoot.querySelectorAll('tabstrip-tab');
        assertEquals(currentWindow.tabs.length, tabElements.length);
      });

  test('removes a tab when tab is removed from current window', () => {
    const tabToRemove = currentWindow.tabs[0];
    callbackRouter.onRemoved.dispatchEvent(tabToRemove.id, {
      windowId: currentWindow.id,
    });
    const tabElements = tabList.shadowRoot.querySelectorAll('tabstrip-tab');
    assertEquals(currentWindow.tabs.length - 1, tabElements.length);
  });

  test('updates a tab with new tab data when a tab is updated', () => {
    const tabToUpdate = currentWindow.tabs[0];
    const changeInfo = {title: 'A new title'};
    const updatedTab = Object.assign({}, tabToUpdate, changeInfo);
    callbackRouter.onUpdated.dispatchEvent(
        tabToUpdate.id, changeInfo, updatedTab);

    const tabElements = tabList.shadowRoot.querySelectorAll('tabstrip-tab');
    assertEquals(tabElements[0].tab, updatedTab);
  });

  test('updates tabs when a new tab is activated', () => {
    const tabElements = tabList.shadowRoot.querySelectorAll('tabstrip-tab');

    // Mock activating the 2nd tab
    callbackRouter.onActivated.dispatchEvent({
      tabId: currentWindow.tabs[1].id,
      windowId: currentWindow.id,
    });
    assertFalse(tabElements[0].tab.active);
    assertTrue(tabElements[1].tab.active);
    assertFalse(tabElements[2].tab.active);
  });
});
