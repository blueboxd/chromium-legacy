// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/app.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {assertNotStyle, assertStyle, createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

/** @return {!newTabPage.mojom.Theme} */
function createTheme() {
  return {
    type: newTabPage.mojom.ThemeType.DEFAULT,
    info: {chromeThemeId: 0},
    backgroundColor: {value: 0xffff0000},
    shortcutBackgroundColor: {value: 0xff00ff00},
    shortcutTextColor: {value: 0xff0000ff},
    isDark: false,
    logoColor: null,
    backgroundImageUrl: null,
    backgroundImageAttribution1: '',
    backgroundImageAttribution2: '',
    backgroundImageAttributionUrl: null,
    dailyRefreshCollectionId: '',
  };
}

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
    testProxy.handler.setResultFor('getDoodle', Promise.resolve({
      doodle: null,
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
    const theme = createTheme();

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertDeepEquals(
        theme, app.shadowRoot.querySelector('ntp-customize-dialog').theme);
  });

  test('setting theme updates ntp', async () => {
    // Act.
    testProxy.callbackRouterRemote.setTheme(createTheme());
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
    assertTrue(app.$.logo.doodleAllowed);
    assertFalse(app.$.logo.singleColored);
  });

  test('open voice search event opens voice search overlay', async () => {
    // Act.
    app.$.fakebox.dispatchEvent(new Event('open-voice-search'));
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
  });

  test('setting background image shows image, disallows doodle', async () => {
    // Arrange.
    const theme = createTheme();
    theme.backgroundImageUrl = {url: 'https://img.png'};

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertNotStyle(app.$.backgroundImage, 'display', 'none');
    assertNotStyle(app.$.backgroundGradient, 'display', 'none');
    assertNotStyle(app.$.backgroundImageAttribution, 'text-shadow', 'none');
    assertEquals(app.$.backgroundImage.path, 'image?https://img.png');
    assertFalse(app.$.logo.doodleAllowed);
  });

  test('setting attributions shows attributions', async function() {
    // Arrange.
    const theme = createTheme();
    theme.backgroundImageAttribution1 = 'foo';
    theme.backgroundImageAttribution2 = 'bar';
    theme.backgroundImageAttributionUrl = {url: 'https://info.com'};

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

  test('setting non-default theme disallows doodle', async function() {
    // Arrange.
    const theme = createTheme();
    theme.type = newTabPage.mojom.ThemeType.CHROME;

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertFalse(app.$.logo.doodleAllowed);
  });

  test('setting logo color colors logo', async function() {
    // Arrange.
    const theme = createTheme();
    theme.logoColor = {value: 0xffff0000};

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertTrue(app.$.logo.singleColored);
    assertStyle(app.$.logo, '--ntp-logo-color', 'rgb(255, 0, 0)');
  });
});
