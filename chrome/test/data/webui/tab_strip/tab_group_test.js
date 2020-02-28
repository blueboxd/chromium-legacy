// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab.js';
import 'chrome://tab-strip/tab_group.js';

suite('TabGroup', () => {
  let tabGroupElement;

  setup(() => {
    document.body.innerHTML = '';
    tabGroupElement = document.createElement('tabstrip-tab-group');
    tabGroupElement.appendChild(document.createElement('tabstrip-tab'));
    document.body.appendChild(tabGroupElement);
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

  test('DraggableChipStaysInPlace', () => {
    const originalChipRect = tabGroupElement.$('#chip').getBoundingClientRect();
    tabGroupElement.setDragging(true);
    const newChipRect = tabGroupElement.$('#chip').getBoundingClientRect();
    assertEquals(originalChipRect.left, newChipRect.left);
    assertEquals(originalChipRect.top, newChipRect.top);
    assertEquals(originalChipRect.right, newChipRect.right);
    assertEquals(originalChipRect.bottom, newChipRect.bottom);
  });

  test('DraggableChipStaysInPlaceInRTL', () => {
    document.documentElement.dir = 'rtl';
    const originalChipRect = tabGroupElement.$('#chip').getBoundingClientRect();
    tabGroupElement.setDragging(true);
    const newChipRect = tabGroupElement.$('#chip').getBoundingClientRect();
    assertEquals(originalChipRect.left, newChipRect.left);
    assertEquals(originalChipRect.top, newChipRect.top);
    assertEquals(originalChipRect.right, newChipRect.right);
    assertEquals(originalChipRect.bottom, newChipRect.bottom);
  });
});
