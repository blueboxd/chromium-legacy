// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, BackgroundSelectionType, BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertNotStyle, assertStyle, createTestProxy, createTheme} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageAppTest', () => {
  /** @type {!AppElement} */
  let app;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      realboxEnabled: false,
    });
  });

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
    testProxy.handler.setResultFor('getOneGoogleBarParts', Promise.resolve({
      parts: null,
    }));
    testProxy.setResultMapperFor('matchMedia', () => ({
                                                 addListener() {},
                                                 removeListener() {},
                                               }));
    testProxy.setResultFor('waitForLazyRender', Promise.resolve());
    BrowserProxy.instance_ = testProxy;

    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await flushTasks();
  });

  test('customize dialog closed on start', () => {
    // Assert.
    assertFalse(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });

  test('clicking customize button opens customize dialog', async () => {
    // Act.
    $$(app, '#customizeButton').click();
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });

  test('setting theme updates customize dialog', async () => {
    // Arrange.
    $$(app, '#customizeButton').click();
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
    assertStyle($$(app, '#background'), 'background-color', 'rgb(255, 0, 0)');
    assertStyle(
        $$(app, '#content'), '--ntp-theme-shortcut-background-color',
        'rgba(0, 255, 0, 255)');
    assertStyle(
        $$(app, '#content'), '--ntp-theme-text-color', 'rgba(0, 0, 255, 255)');
    assertFalse($$(app, '#background').hasAttribute('has-background-image'));
    assertStyle($$(app, '#backgroundImage'), 'display', 'none');
    assertStyle($$(app, '#backgroundGradient'), 'display', 'none');
    assertStyle($$(app, '#backgroundImageAttribution'), 'display', 'none');
    assertStyle($$(app, '#backgroundImageAttribution2'), 'display', 'none');
    assertTrue($$(app, '#logo').doodleAllowed);
    assertFalse($$(app, '#logo').singleColored);
  });

  test('realbox is not visible by default', async () => {
    // Assert.
    assertNotStyle($$(app, '#fakebox'), 'display', 'none');
    assertStyle($$(app, '#realbox'), 'display', 'none');
  });

  test('open voice search event opens voice search overlay', async () => {
    // Act.
    $$(app, '#fakebox').dispatchEvent(new Event('open-voice-search'));
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
    assertNotStyle($$(app, '#backgroundImage'), 'display', 'none');
    assertNotStyle($$(app, '#backgroundGradient'), 'display', 'none');
    assertNotStyle(
        $$(app, '#backgroundImageAttribution'), 'text-shadow', 'none');
    assertEquals(
        $$(app, '#backgroundImage').path, 'background_image?https://img.png');
    assertFalse($$(app, '#logo').doodleAllowed);
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
    assertNotStyle($$(app, '#backgroundImageAttribution'), 'display', 'none');
    assertNotStyle($$(app, '#backgroundImageAttribution2'), 'display', 'none');
    assertEquals(
        $$(app, '#backgroundImageAttribution').getAttribute('href'),
        'https://info.com');
    assertEquals(
        $$(app, '#backgroundImageAttribution1').textContent.trim(), 'foo');
    assertEquals(
        $$(app, '#backgroundImageAttribution2').textContent.trim(), 'bar');
  });

  test('setting non-default theme disallows doodle', async function() {
    // Arrange.
    const theme = createTheme();
    theme.type = newTabPage.mojom.ThemeType.CHROME;

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertFalse($$(app, '#logo').doodleAllowed);
  });

  test('setting logo color colors logo', async function() {
    // Arrange.
    const theme = createTheme();
    theme.logoColor = {value: 0xffff0000};

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertTrue($$(app, '#logo').singleColored);
    assertStyle($$(app, '#logo'), '--ntp-logo-color', 'rgba(255, 0, 0, 255)');
  });

  test('preview background image', async () => {
    const theme = createTheme();
    theme.backgroundImageUrl = {url: 'https://example.com/image.png'};
    theme.backgroundImageAttribution1 = 'foo';
    theme.backgroundImageAttribution2 = 'bar';
    theme.backgroundImageAttributionUrl = {url: 'https://info.com'};
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertEquals(
        'background_image?https://example.com/image.png',
        $$(app, '#backgroundImage').path);
    assertEquals(
        'https://info.com/', $$(app, '#backgroundImageAttribution').href);
    assertEquals('foo', $$(app, '#backgroundImageAttribution1').innerText);
    assertEquals('bar', $$(app, '#backgroundImageAttribution2').innerText);
    $$(app, '#customizeButton').click();
    await flushTasks();
    const dialog = app.shadowRoot.querySelector('ntp-customize-dialog');
    dialog.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: {
        attribution1: '1',
        attribution2: '2',
        attributionUrl: {url: 'https://example.com'},
        imageUrl: {url: 'https://example.com/other.png'},
      },
    };
    assertEquals(
        'background_image?https://example.com/other.png',
        $$(app, '#backgroundImage').path);
    assertEquals(
        'https://example.com/', $$(app, '#backgroundImageAttribution').href);
    assertEquals('1', $$(app, '#backgroundImageAttribution1').innerText);
    assertEquals('2', $$(app, '#backgroundImageAttribution2').innerText);
    assertFalse($$(app, '#backgroundImageAttribution2').hidden);

    dialog.backgroundSelection = {type: BackgroundSelectionType.NO_SELECTION};
    assertEquals(
        'background_image?https://example.com/image.png',
        $$(app, '#backgroundImage').path);
  });

  test('theme update when dialog closed clears selection', async () => {
    const theme = createTheme();
    theme.backgroundImageUrl = {url: 'https://example.com/image.png'};
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertEquals(
        'background_image?https://example.com/image.png',
        $$(app, '#backgroundImage').path);
    $$(app, '#customizeButton').click();
    await flushTasks();
    const dialog = app.shadowRoot.querySelector('ntp-customize-dialog');
    dialog.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: {
        attribution1: '1',
        attribution2: '2',
        attributionUrl: {url: 'https://example.com'},
        imageUrl: {url: 'https://example.com/other.png'},
      },
    };
    assertEquals(
        'background_image?https://example.com/other.png',
        $$(app, '#backgroundImage').path);
    dialog.dispatchEvent(new Event('close'));
    assertEquals(
        'background_image?https://example.com/other.png',
        $$(app, '#backgroundImage').path);
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertEquals(
        'background_image?https://example.com/image.png',
        $$(app, '#backgroundImage').path);
  });

  test('theme updates add shortcut color', async () => {
    const theme = createTheme();
    theme.shortcutUseWhiteAddIcon = true;
    testProxy.callbackRouterRemote.setTheme(theme);
    assertFalse(app.$.mostVisited.hasAttribute('use-white-add-icon'));
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(app.$.mostVisited.hasAttribute('use-white-add-icon'));
  });
});
