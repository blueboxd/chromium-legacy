// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab.js';

suite('Tab', function() {
  let tabElement;

  setup(() => {
    document.body.innerHTML = '';

    tabElement = document.createElement('tabstrip-tab');
    document.body.appendChild(tabElement);
  });

  test('sets the tabindex', () => {
    assertEquals(tabElement.getAttribute('tabindex'), '0');
  });

  test('sets the title', () => {
    const expectedTitle = 'My title';
    tabElement.tab = {title: expectedTitle};
    assertEquals(
        expectedTitle,
        tabElement.shadowRoot.querySelector('#titleText').innerText);
  });

  test('exposes the tab ID to an attribute', () => {
    tabElement.tab = {id: 1001};
    assertEquals('1001', tabElement.getAttribute('data-tab-id'));
  });
});
