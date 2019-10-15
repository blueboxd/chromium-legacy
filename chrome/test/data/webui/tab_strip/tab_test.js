// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab.js';

import {getFavicon} from 'chrome://resources/js/icon.m.js';
import {TabNetworkState, TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';

import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('Tab', function() {
  let testTabsApiProxy;
  let tabElement;

  const tab = {
    id: 1001,
    networkState: TabNetworkState.NONE,
    title: 'My title',
  };

  setup(() => {
    document.body.innerHTML = '';

    testTabsApiProxy = new TestTabsApiProxy();
    TabsApiProxy.instance_ = testTabsApiProxy;

    tabElement = document.createElement('tabstrip-tab');
    tabElement.tab = tab;
    document.body.appendChild(tabElement);
  });

  test('slideIn animates in the element', async () => {
    const animationPromise = tabElement.slideIn();
    // Before animation completes
    assertEquals('0', window.getComputedStyle(tabElement).opacity);
    assertEquals('0px', window.getComputedStyle(tabElement).maxWidth);
    await animationPromise;
    // After animation completes
    assertEquals('1', window.getComputedStyle(tabElement).opacity);
    assertEquals('280px', window.getComputedStyle(tabElement).maxWidth);
  });

  test('slideOut animates out the element', async () => {
    const animationPromise = tabElement.slideOut();
    // Before animation completes
    assertEquals('1', window.getComputedStyle(tabElement).opacity);
    assertEquals('280px', window.getComputedStyle(tabElement).maxWidth);
    await animationPromise;
    // After animation completes
    assertFalse(document.body.contains(tabElement));
  });

  test('toggles an [active] attribute when active', () => {
    tabElement.tab = Object.assign({}, tab, {active: true});
    assertTrue(tabElement.hasAttribute('active'));
    tabElement.tab = Object.assign({}, tab, {active: false});
    assertFalse(tabElement.hasAttribute('active'));
  });

  test(
      'toggles a [hide-icon_] attribute when the icon container should be ' +
          'shown',
      () => {
        tabElement.tab = Object.assign({}, tab, {showIcon: true});
        assertFalse(tabElement.hasAttribute('hide-icon_'));
        tabElement.tab = Object.assign({}, tab, {showIcon: false});
        assertTrue(tabElement.hasAttribute('hide-icon_'));
      });

  test('toggles a [pinned_] attribute when pinned', () => {
    tabElement.tab = Object.assign({}, tab, {pinned: true});
    assertTrue(tabElement.hasAttribute('pinned_'));
    tabElement.tab = Object.assign({}, tab, {pinned: false});
    assertFalse(tabElement.hasAttribute('pinned_'));
  });

  test('toggles a [loading_] attribute when loading', () => {
    tabElement.tab =
        Object.assign({}, tab, {networkState: TabNetworkState.LOADING});
    assertTrue(tabElement.hasAttribute('loading_'));
    tabElement.tab =
        Object.assign({}, tab, {networkState: TabNetworkState.NONE});
    assertFalse(tabElement.hasAttribute('loading_'));
  });

  test('toggles a [waiting_] attribute when waiting', () => {
    tabElement.tab =
        Object.assign({}, tab, {networkState: TabNetworkState.WAITING});
    assertTrue(tabElement.hasAttribute('waiting_'));
    tabElement.tab =
        Object.assign({}, tab, {networkState: TabNetworkState.NONE});
    assertFalse(tabElement.hasAttribute('waiting_'));
  });

  test('toggles a [blocked_] attribute when blocked', () => {
    tabElement.tab = Object.assign({}, tab, {blocked: true});
    assertTrue(tabElement.hasAttribute('blocked_'));
    tabElement.tab = Object.assign({}, tab, {blocked: false});
    assertFalse(tabElement.hasAttribute('blocked_'));
  });

  test('toggles a [crashed] attribute when crashed', () => {
    tabElement.tab = Object.assign({}, tab, {crashed: true});
    assertTrue(tabElement.hasAttribute('crashed'));
    tabElement.tab = Object.assign({}, tab, {crashed: false});
    assertFalse(tabElement.hasAttribute('crashed'));
  });

  test('clicking on the element activates the tab', () => {
    tabElement.click();
    return testTabsApiProxy.whenCalled('activateTab', tabId => {
      assertEquals(tabId, tab.id);
    });
  });

  test('sets the title', () => {
    assertEquals(
        tab.title, tabElement.shadowRoot.querySelector('#titleText').innerText);
  });

  test('exposes the tab ID to an attribute', () => {
    tabElement.tab = Object.assign({}, tab, {id: 1001});
    assertEquals('1001', tabElement.getAttribute('data-tab-id'));
  });

  test('closes the tab', () => {
    tabElement.shadowRoot.querySelector('#close').click();
    return testTabsApiProxy.whenCalled('closeTab').then(tabId => {
      assertEquals(tabId, tab.id);
    });
  });

  test('sets the favicon to the favicon URL', () => {
    const expectedFaviconUrl = 'data:mock-favicon';
    tabElement.tab = Object.assign({}, tab, {favIconUrl: expectedFaviconUrl});
    const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
    assertEquals(
        faviconElement.style.backgroundImage, `url("${expectedFaviconUrl}")`);
  });

  test(
      'sets the favicon to the default favicon URL if there is none provided',
      () => {
        const updatedTab = Object.assign({}, tab);
        delete updatedTab.favIconUrl;
        tabElement.tab = updatedTab;
        const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
        assertEquals(faviconElement.style.backgroundImage, getFavicon(''));
      });

  test('removes the favicon if the tab is waiting', () => {
    tabElement.tab = Object.assign({}, tab, {
      favIconUrl: 'data:mock-favicon',
      networkState: TabNetworkState.WAITING,
    });
    const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
    assertEquals(faviconElement.style.backgroundImage, 'none');
  });

  test(
      'removes the favicon if the tab is loading with a default favicon',
      () => {
        tabElement.tab = Object.assign({}, tab, {
          favIconUrl: 'data:mock-favicon',
          hasDefaultFavicon: true,
          networkState: TabNetworkState.WAITING,
        });
        const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
        assertEquals(faviconElement.style.backgroundImage, 'none');
      });

  test('hides the thumbnail if there is no source yet', () => {
    const thumbnailImage = tabElement.shadowRoot.querySelector('#thumbnailImg');
    assertFalse(thumbnailImage.hasAttribute('src'));
    assertEquals(window.getComputedStyle(thumbnailImage).display, 'none');
  });

  test('tracks and updates the thumbnail source', async () => {
    const requestedTabId =
        await testTabsApiProxy.whenCalled('trackThumbnailForTab');
    assertEquals(requestedTabId, tab.id);

    const thumbnailSource = 'data:mock-thumbnail-source';
    tabElement.updateThumbnail(thumbnailSource);
    assertEquals(
        tabElement.shadowRoot.querySelector('#thumbnailImg').src,
        thumbnailSource);
  });

  test('setting dragging state toggles an attribute', () => {
    tabElement.setDragging(true);
    assertTrue(tabElement.hasAttribute('dragging_'));
    tabElement.setDragging(false);
    assertFalse(tabElement.hasAttribute('dragging_'));
  });

  test('getting the drag image grabs the contents', () => {
    assertEquals(
        tabElement.getDragImage(),
        tabElement.shadowRoot.querySelector('#dragImage'));
  });

  test('has custom context menu', async () => {
    let event = new Event('contextmenu');
    event.clientX = 1;
    event.clientY = 2;
    tabElement.dispatchEvent(event);

    const contextMenuArgs =
        await testTabsApiProxy.whenCalled('showTabContextMenu');
    assertEquals(contextMenuArgs[0], tabElement.tab.id);
    assertEquals(contextMenuArgs[1], 1);
    assertEquals(contextMenuArgs[2], 2);
  });
});
