// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<history-synced-device-manager>', function() {
  let element;

  setup(function() {
    element = document.createElement('history-synced-device-manager');
    element.signInState = true;
    element.searchTerm = '';
    replaceBody(element);
  });

  test('focus and keyboard nav', async () => {
    const sessionList = [
      createSession(
          'Nexus 5',
          [createWindow(['http://www.example.com', 'http://www.google.com'])]),
      createSession('Pixel C', [createWindow(['http://www.badssl.com'])]),
      createSession('Potato', [createWindow(['http://www.wikipedia.org'])]),
    ];

    element.sessionList = sessionList;

    let lastFocused;
    let cards;
    let focused;
    const onFocusHandler = element.focusGrid_.onFocus;
    element.focusGrid_.onFocus = function(row, e) {
      onFocusHandler.call(element.focusGrid_, row, e);
      lastFocused = e.currentTarget;
    };

    await test_util.flushTasks();
    cards = polymerSelectAll(element, 'history-synced-device-card');

    focused = cards[0].$['menu-button'];
    focused.focus();

    // Go to the collapse button.
    MockInteractions.pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    focused = cards[0].$['collapse-button'];
    assertEquals(focused, lastFocused);

    // Go to the first url.
    MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused = polymerSelectAll(cards[0], '.website-link')[0];
    assertEquals(focused, lastFocused);

    // Collapse the first card.
    MockInteractions.pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    focused = cards[0].$['collapse-button'];
    assertEquals(focused, lastFocused);
    MockInteractions.tap(focused);
    await test_util.flushTasks();

    // Pressing down goes to the next card.
    MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused = cards[1].$['collapse-button'];
    assertEquals(focused, lastFocused);

    // Expand the first card.
    MockInteractions.pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    focused = cards[0].$['collapse-button'];
    assertEquals(focused, lastFocused);
    MockInteractions.tap(focused);
    await test_util.flushTasks();

    // First card's urls are focusable again.
    MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused = polymerSelectAll(cards[0], '.website-link')[0];
    assertEquals(focused, lastFocused);

    // Remove the second URL from the first card.
    sessionList[0].windows[0].tabs.splice(1, 1);
    element.sessionList = sessionList.slice();
    await test_util.flushTasks();

    cards = polymerSelectAll(element, 'history-synced-device-card');

    // Go to the next card's menu buttons.
    MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused = cards[1].$['collapse-button'];
    assertEquals(focused, lastFocused);

    MockInteractions.pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    focused = polymerSelectAll(cards[0], '.website-link')[0];
    assertEquals(focused, lastFocused);

    // Remove the second card.
    sessionList.splice(1, 1);
    element.sessionList = sessionList.slice();
    await test_util.flushTasks();

    cards = polymerSelectAll(element, 'history-synced-device-card');

    // Pressing down goes to the next card.
    MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused = cards[1].$['collapse-button'];
    assertEquals(focused, lastFocused);
  });
});
