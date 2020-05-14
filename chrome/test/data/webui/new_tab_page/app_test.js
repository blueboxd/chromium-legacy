// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, BackgroundManager, BackgroundSelectionType, BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertNotStyle, assertStyle, createTestProxy, createTheme} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageAppTest', () => {
  /** @type {!AppElement} */
  let app;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  /**
   * @implements {BackgroundManager}
   * @extends {TestBrowserProxy}
   */
  let backgroundManager;

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
    backgroundManager = TestBrowserProxy.fromClass(BackgroundManager);
    backgroundManager.setResultFor(
        'getBackgroundImageLoadTime', Promise.resolve(0));
    BackgroundManager.instance_ = backgroundManager;

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
    assertEquals(1, backgroundManager.getCallCount('setBackgroundColor'));
    assertEquals(
        0xffff0000 /* red */,
        (await backgroundManager.whenCalled('setBackgroundColor')).value);
    assertStyle(
        $$(app, '#content'), '--ntp-theme-shortcut-background-color',
        'rgba(0, 255, 0, 255)');
    assertStyle(
        $$(app, '#content'), '--ntp-theme-text-color', 'rgba(0, 0, 255, 255)');
    assertEquals(1, backgroundManager.getCallCount('setShowBackgroundImage'));
    assertFalse(await backgroundManager.whenCalled('setShowBackgroundImage'));
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
    backgroundManager.resetResolver('setShowBackgroundImage');
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, backgroundManager.getCallCount('setShowBackgroundImage'));
    assertTrue(await backgroundManager.whenCalled('setShowBackgroundImage'));
    assertNotStyle(
        $$(app, '#backgroundImageAttribution'), 'text-shadow', 'none');
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImageUrl'));
    assertEquals(
        'https://img.png',
        await backgroundManager.whenCalled('setBackgroundImageUrl'));
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
        'https://info.com',
        $$(app, '#backgroundImageAttribution').getAttribute('href'));
    assertEquals(
        'foo', $$(app, '#backgroundImageAttribution1').textContent.trim());
    assertEquals(
        'bar', $$(app, '#backgroundImageAttribution2').textContent.trim());
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
    assertEquals(backgroundManager.getCallCount('setBackgroundImageUrl'), 1);
    assertEquals(
        'https://example.com/image.png',
        await backgroundManager.whenCalled('setBackgroundImageUrl'));
    assertEquals(
        'https://info.com/', $$(app, '#backgroundImageAttribution').href);
    assertEquals('foo', $$(app, '#backgroundImageAttribution1').innerText);
    assertEquals('bar', $$(app, '#backgroundImageAttribution2').innerText);
    $$(app, '#customizeButton').click();
    await flushTasks();
    const dialog = app.shadowRoot.querySelector('ntp-customize-dialog');
    backgroundManager.resetResolver('setBackgroundImageUrl');
    dialog.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: {
        attribution1: '1',
        attribution2: '2',
        attributionUrl: {url: 'https://example.com'},
        imageUrl: {url: 'https://example.com/other.png'},
      },
    };
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImageUrl'));
    assertEquals(
        'https://example.com/other.png',
        await backgroundManager.whenCalled('setBackgroundImageUrl'));
    assertEquals(
        'https://example.com/', $$(app, '#backgroundImageAttribution').href);
    assertEquals('1', $$(app, '#backgroundImageAttribution1').innerText);
    assertEquals('2', $$(app, '#backgroundImageAttribution2').innerText);
    assertFalse($$(app, '#backgroundImageAttribution2').hidden);

    backgroundManager.resetResolver('setBackgroundImageUrl');
    dialog.backgroundSelection = {type: BackgroundSelectionType.NO_SELECTION};
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImageUrl'));
    assertEquals(
        'https://example.com/image.png',
        await backgroundManager.whenCalled('setBackgroundImageUrl'));
  });

  test('theme update when dialog closed clears selection', async () => {
    const theme = createTheme();
    theme.backgroundImageUrl = {url: 'https://example.com/image.png'};
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImageUrl'));
    assertEquals(
        'https://example.com/image.png',
        await backgroundManager.whenCalled('setBackgroundImageUrl'));
    $$(app, '#customizeButton').click();
    await flushTasks();
    const dialog = app.shadowRoot.querySelector('ntp-customize-dialog');
    backgroundManager.resetResolver('setBackgroundImageUrl');
    dialog.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: {
        attribution1: '1',
        attribution2: '2',
        attributionUrl: {url: 'https://example.com'},
        imageUrl: {url: 'https://example.com/other.png'},
      },
    };
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImageUrl'));
    assertEquals(
        'https://example.com/other.png',
        await backgroundManager.whenCalled('setBackgroundImageUrl'));
    backgroundManager.resetResolver('setBackgroundImageUrl');
    dialog.dispatchEvent(new Event('close'));
    assertEquals(0, backgroundManager.getCallCount('setBackgroundImageUrl'));
    backgroundManager.resetResolver('setBackgroundImageUrl');
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertEquals(2, backgroundManager.getCallCount('setBackgroundImageUrl'));
    assertEquals(
        'https://example.com/image.png',
        await backgroundManager.whenCalled('setBackgroundImageUrl'));
  });

  test('theme updates add shortcut color', async () => {
    const theme = createTheme();
    theme.shortcutUseWhiteAddIcon = true;
    testProxy.callbackRouterRemote.setTheme(theme);
    assertFalse(app.$.mostVisited.hasAttribute('use-white-add-icon'));
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(app.$.mostVisited.hasAttribute('use-white-add-icon'));
  });

  test('theme updates use title pill', async () => {
    const theme = createTheme();
    theme.shortcutUseTitlePill = true;
    testProxy.callbackRouterRemote.setTheme(theme);
    assertFalse(app.$.mostVisited.hasAttribute('use-title-pill'));
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(app.$.mostVisited.hasAttribute('use-title-pill'));
  });
});
