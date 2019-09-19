// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab.js';

import {getFavicon, getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';
import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

suite('Tab', function() {
  let testTabsApiProxy;
  let tabElement;

  const tab = {
    id: 1001,
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

  test('toggles a [pinned] attribute when pinned', () => {
    tabElement.tab = Object.assign({}, tab, {pinned: true});
    assertTrue(tabElement.hasAttribute('pinned'));
    tabElement.tab = Object.assign({}, tab, {pinned: false});
    assertFalse(tabElement.hasAttribute('pinned'));
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
    const expectedFaviconUrl = 'http://google.com/favicon.ico';
    tabElement.tab = Object.assign({}, tab, {favIconUrl: expectedFaviconUrl});
    const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
    assertEquals(
        faviconElement.style.backgroundImage, getFavicon(expectedFaviconUrl));
  });

  test('sets the favicon to the page URL if favicon URL does not exist', () => {
    const expectedPageUrl = 'http://google.com';
    tabElement.tab = Object.assign({}, tab, {url: expectedPageUrl});
    const faviconElement = tabElement.shadowRoot.querySelector('#favicon');
    assertEquals(
        faviconElement.style.backgroundImage,
        getFaviconForPageURL(expectedPageUrl, false));
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
    assertTrue(tabElement.hasAttribute('dragging'));
    tabElement.setDragging(false);
    assertFalse(tabElement.hasAttribute('dragging'));
  });

  test('getting the drag image grabs the contents', () => {
    assertEquals(
        tabElement.getDragImage(),
        tabElement.shadowRoot.querySelector('#dragImage'));
  });
});
