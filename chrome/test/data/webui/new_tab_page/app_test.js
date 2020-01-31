// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/app.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {assertStyle, TestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageAppTest', () => {
  /** @type {!AppElement} */
  let app;

  /** @type {TestProxy} */
  let testProxy;

  setup(async () => {
    PolymerTest.clearBody();

    testProxy = new TestProxy();
    BrowserProxy.instance_ = testProxy;

    app = document.createElement('ntp-app');
    document.body.appendChild(app);
  });

  test('customize dialog closed on start', () => {
    // Assert.
    assertFalse(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });

  test('clicking customize button opens customize dialog', async () => {
    // Act.
    app.$.customizeButton.click();
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });

  test('setting theme updates customize dialog', async () => {
    // Arrange.
    app.$.customizeButton.click();
    const theme = {
      type: newTabPage.mojom.ThemeType.DEFAULT,
      info: {chromeThemeId: 0},
      backgroundColor: {value: 0xffff0000},
      shortcutBackgroundColor: {value: 0xff00ff00},
      shortcutTextColor: {value: 0xff0000ff},
      isDark: false,
    };

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertDeepEquals(
        theme, app.shadowRoot.querySelector('ntp-customize-dialog').theme);
  });

  test('setting theme updates background color and most visited', async () => {
    // Act.
    testProxy.callbackRouterRemote.setTheme({
      type: newTabPage.mojom.ThemeType.DEFAULT,
      info: {chromeThemeId: 0},
      backgroundColor: {value: 0xffff0000},
      shortcutBackgroundColor: {value: 0xff00ff00},
      shortcutTextColor: {value: 0xff0000ff},
      isDark: false,
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertStyle(app.$.background, 'background-color', 'rgb(255, 0, 0)');
    assertStyle(
        app.shadowRoot.querySelector('ntp-most-visited'),
        '--icon-background-color', 'rgb(0, 255, 0)');
    assertStyle(
        app.shadowRoot.querySelector('ntp-most-visited'), '--tile-title-color',
        'rgb(0, 0, 255)');
  });

  test('clicking voice search button opens voice search overlay', async () => {
    // Act.
    app.$.voiceSearchButton.click();
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
  });
});
