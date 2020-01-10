// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/customize_themes.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {assertFocus, keydown, TestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageCustomizeThemesFocusTest', () => {
  /** @type {!customizeThemesElement} */
  let customizeThemes;

  /** @type {TestProxy} */
  let testProxy;

  function queryThemeIcons() {
    return customizeThemes.shadowRoot.querySelectorAll(
        '#themesContainer ntp-theme-icon');
  }

  setup(async () => {
    PolymerTest.clearBody();

    testProxy = new TestProxy();
    BrowserProxy.instance_ = testProxy;

    const colors = {frame: {value: 0xff000000}, activeTab: {value: 0xff0000ff}};
    const themes = [];
    for (let i = 0; i < 10; ++i) {
      themes.push({id: i, label: `theme_${i}`, colors});
    }
    testProxy.handler.setResultFor('getChromeThemes', Promise.resolve({
      chromeThemes: themes,
    }));
    customizeThemes = document.createElement('ntp-customize-themes');
    document.body.appendChild(customizeThemes);
    await flushTasks();
  });

  test('right focuses right theme icon', async () => {
    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[0], 'ArrowRight');

    // Assert.
    assertFocus(themeIcons[1]);
  });

  test('right wrap around focuses first theme icon', async () => {
    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[themeIcons.length - 1], 'ArrowRight');

    // Assert.
    assertFocus(themeIcons[0]);
  });

  test('left focuses left theme icon', async () => {
    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[1], 'ArrowLeft');

    // Assert.
    assertFocus(themeIcons[0]);
  });

  test('left wrap around focuses last theme icon', async () => {
    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[0], 'ArrowLeft');

    // Assert.
    assertFocus(themeIcons[themeIcons.length - 1]);
  });

  test('right focuses left theme icon in RTL', async () => {
    // Arrange.
    customizeThemes.dir = 'rtl';

    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[1], 'ArrowRight');

    // Assert.
    assertFocus(themeIcons[0]);
  });

  test('right wrap around focuses last theme icon in RTL', async () => {
    // Arrange.
    customizeThemes.dir = 'rtl';

    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[0], 'ArrowRight');

    // Assert.
    assertFocus(themeIcons[themeIcons.length - 1]);
  });

  test('left focuses right theme icon in RTL', async () => {
    // Arrange.
    customizeThemes.dir = 'rtl';

    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[0], 'ArrowLeft');

    // Assert.
    assertFocus(themeIcons[1]);
  });

  test('left wrap around focuses first theme icon in RTL', async () => {
    // Arrange.
    customizeThemes.dir = 'rtl';

    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[themeIcons.length - 1], 'ArrowLeft');

    // Assert.
    assertFocus(themeIcons[0]);
  });

  test('down focuses below theme icon', async () => {
    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[0], 'ArrowDown');

    // Assert.
    assertFocus(themeIcons[6]);
  });

  test('up focuses above theme icon', async () => {
    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[6], 'ArrowUp');

    // Assert.
    assertFocus(themeIcons[0]);
  });

  test('down wrap around focuses top theme icon', async () => {
    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[6], 'ArrowDown');

    // Assert.
    assertFocus(themeIcons[0]);
  });

  test('up wrap around focuses bottom theme icon', async () => {
    // Act.
    const themeIcons = queryThemeIcons();
    keydown(themeIcons[0], 'ArrowDown');

    // Assert.
    assertFocus(themeIcons[6]);
  });

  test('enter clicks focused theme icon', async () => {
    // Arrange.
    const themeIcon = queryThemeIcons()[0];
    themeIcon.focus();
    const themeIconClicked = eventToPromise('click', themeIcon);

    // Act.
    keydown(themeIcon, 'Enter');

    // Assert.
    await themeIconClicked;
  });

  test('space clicks focused theme icon', async () => {
    // Arrange.
    const themeIcon = queryThemeIcons()[0];
    themeIcon.focus();
    const themeIconClicked = eventToPromise('click', themeIcon);

    // Act.
    keydown(themeIcon, ' ');

    // Assert.
    await themeIconClicked;
  });
});
