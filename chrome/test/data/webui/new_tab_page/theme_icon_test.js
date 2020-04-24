// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import {assertStyle} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageThemeIconTest', () => {
  /** @type {!ThemeIconElement} */
  let themeIcon;

  /**
   * @return {!NodeList<!HTMLElement>}
   * @private
   */
  function queryAll(selector) {
    return themeIcon.shadowRoot.querySelectorAll(selector);
  }

  /**
   * @return {!HTMLElement}
   * @private
   */
  function query(selector) {
    return themeIcon.shadowRoot.querySelector(selector);
  }

  setup(() => {
    PolymerTest.clearBody();

    themeIcon = document.createElement('ntp-theme-icon');
    document.body.appendChild(themeIcon);
  });

  test('setting frame color sets stroke and gradient', () => {
    // Act.
    themeIcon.style.setProperty('--ntp-theme-icon-frame-color', 'red');

    // Assert.
    assertStyle(query('#circle'), 'stroke', 'rgb(255, 0, 0)');
    assertStyle(
        queryAll('#gradient > stop')[1], 'stop-color', 'rgb(255, 0, 0)');
  });

  test('setting active tab color sets gradient', async () => {
    // Act.
    themeIcon.style.setProperty('--ntp-theme-icon-active-tab-color', 'red');

    // Assert.
    assertStyle(
        queryAll('#gradient > stop')[0], 'stop-color', 'rgb(255, 0, 0)');
  });

  test('setting explicit stroke color sets different stroke', async () => {
    // Act.
    themeIcon.style.setProperty('--ntp-theme-icon-frame-color', 'red');
    themeIcon.style.setProperty('--ntp-theme-icon-stroke-color', 'blue');

    // Assert.
    assertStyle(query('#circle'), 'stroke', 'rgb(0, 0, 255)');
    assertStyle(
        queryAll('#gradient > stop')[1], 'stop-color', 'rgb(255, 0, 0)');
  });

  test('selecting icon shows ring and check mark', async () => {
    // Act.
    themeIcon.setAttribute('selected', true);

    // Assert.
    assertStyle(query('#ring'), 'visibility', 'visible');
    assertStyle(query('#checkMark'), 'visibility', 'visible');
  });
});
