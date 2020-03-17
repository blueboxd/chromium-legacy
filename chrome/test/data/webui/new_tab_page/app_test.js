// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/app.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {assertNotStyle, assertStyle, createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageAppTest', () => {
  /** @type {!AppElement} */
  let app;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(async () => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    testProxy.handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    testProxy.handler.setResultFor('getChromeThemes', Promise.resolve({
      chromeThemes: [],
    }));
    testProxy.setResultMapperFor('matchMedia', () => ({
                                                 addListener() {},
                                                 removeListener() {},
                                               }));
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
      backgroundImageUrl: null,
      backgroundImageAttribution1: '',
      backgroundImageAttribution2: '',
      backgroundImageAttributionUrl: null,
    };

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertDeepEquals(
        theme, app.shadowRoot.querySelector('ntp-customize-dialog').theme);
  });

  test('setting theme updates ntp', async () => {
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
        app.$.background, '--ntp-theme-shortcut-background-color',
        'rgb(0, 255, 0)');
    assertStyle(app.$.background, '--ntp-theme-text-color', 'rgb(0, 0, 255)');
    assertFalse(app.$.background.hasAttribute('has-background-image'));
    assertStyle(app.$.backgroundImage, 'display', 'none');
    assertStyle(app.$.backgroundGradient, 'display', 'none');
    assertStyle(app.$.backgroundImageAttribution, 'display', 'none');
    assertStyle(app.$.backgroundImageAttribution2, 'display', 'none');
  });

  test('open voice search event opens voice search overlay', async () => {
    // Act.
    app.$.fakebox.dispatchEvent(new Event('open-voice-search'));
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
  });

  test('setting background images shows iframe and gradient', async () => {
    // Act.
    const theme = {
      type: newTabPage.mojom.ThemeType.DEFAULT,
      info: {chromeThemeId: 0},
      backgroundColor: {value: 0xffff0000},
      shortcutBackgroundColor: {value: 0xff00ff00},
      shortcutTextColor: {value: 0xff0000ff},
      isDark: false,
      backgroundImageUrl: {url: 'https://img.png'},
      backgroundImageAttribution1: '',
      backgroundImageAttribution2: '',
      backgroundImageAttributionUrl: null,
    };

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertNotStyle(app.$.backgroundImage, 'display', 'none');
    assertNotStyle(app.$.backgroundGradient, 'display', 'none');
    assertNotStyle(app.$.backgroundImageAttribution, 'text-shadow', 'none');
    assertEquals(app.$.backgroundImage.path, 'image?https://img.png');
  });

  test('setting attributions shows attributions', async function() {
    // Act.
    const theme = {
      type: newTabPage.mojom.ThemeType.DEFAULT,
      info: {chromeThemeId: 0},
      backgroundColor: {value: 0xffff0000},
      shortcutBackgroundColor: {value: 0xff00ff00},
      shortcutTextColor: {value: 0xff0000ff},
      isDark: false,
      backgroundImageUrl: null,
      backgroundImageAttribution1: 'foo',
      backgroundImageAttribution2: 'bar',
      backgroundImageAttributionUrl: {url: 'https://info.com'},
    };

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertNotStyle(app.$.backgroundImageAttribution, 'display', 'none');
    assertNotStyle(app.$.backgroundImageAttribution2, 'display', 'none');
    assertEquals(
        app.$.backgroundImageAttribution.getAttribute('href'),
        'https://info.com');
    assertEquals(app.$.backgroundImageAttribution1.textContent.trim(), 'foo');
    assertEquals(app.$.backgroundImageAttribution2.textContent.trim(), 'bar');
  });
});
