// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab.js';
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

  test('toggles an [active] attribute when active', () => {
    tabElement.tab = Object.assign({}, tab, {active: true});
    assertTrue(tabElement.hasAttribute('active'));
    tabElement.tab = Object.assign({}, tab, {active: false});
    assertFalse(tabElement.hasAttribute('active'));
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
});
