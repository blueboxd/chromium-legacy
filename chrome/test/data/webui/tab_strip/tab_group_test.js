// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab_group.js';

suite('TabGroup', () => {
  let tabGroupElement;

  setup(() => {
    document.body.innerHTML = '';
    tabGroupElement = document.createElement('tabstrip-tab-group');
  });

  test('UpdatesVisuals', () => {
    const visuals = {
      color: '255, 0, 0',
      textColor: '0, 0, 255',
      title: 'My new title',
    };
    tabGroupElement.updateVisuals(visuals);
    assertEquals(
        visuals.title,
        tabGroupElement.shadowRoot.querySelector('#title').innerText);
    assertEquals(
        visuals.color,
        tabGroupElement.style.getPropertyValue(
            '--tabstrip-tab-group-color-rgb'));
    assertEquals(
        visuals.textColor,
        tabGroupElement.style.getPropertyValue(
            '--tabstrip-tab-group-text-color-rgb'));
  });
});
