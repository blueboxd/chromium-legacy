// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<history-list>', function() {
  let app;
  let element;
  const TEST_HISTORY_RESULTS = [
    createHistoryEntry('2016-03-15', 'https://www.google.com'),
    createHistoryEntry('2016-03-14 10:00', 'https://www.example.com'),
    createHistoryEntry('2016-03-14 9:00', 'https://www.google.com'),
    createHistoryEntry('2016-03-13', 'https://en.wikipedia.org')
  ];
  TEST_HISTORY_RESULTS[2].starred = true;

  setup(function() {
    app = replaceApp();
    element = app.$.history;
    return test_util.flushTasks();
  });

  test('list focus and keyboard nav', async () => {
    app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
    let focused;
    await test_util.flushTasks();
    Polymer.dom.flush();
    const items = polymerSelectAll(element, 'history-item');

    items[2].$.checkbox.focus();
    focused = items[2].$.checkbox.getFocusableElement();

    // Wait for next render to ensure that focus handlers have been
    // registered (see HistoryItemElement.attached).
    await test_util.waitAfterNextRender(this);

    MockInteractions.pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    Polymer.dom.flush();
    focused = items[2].$.link;
    assertEquals(focused, element.lastFocused_);
    assertTrue(items[2].row_.isActive());
    assertFalse(items[3].row_.isActive());

    MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    Polymer.dom.flush();
    focused = items[3].$.link;
    assertEquals(focused, element.lastFocused_);
    assertFalse(items[2].row_.isActive());
    assertTrue(items[3].row_.isActive());

    MockInteractions.pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    Polymer.dom.flush();
    focused = items[3].$['menu-button'];
    assertEquals(focused, element.lastFocused_);
    assertFalse(items[2].row_.isActive());
    assertTrue(items[3].row_.isActive());

    MockInteractions.pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    Polymer.dom.flush();
    focused = items[2].$['menu-button'];
    assertEquals(focused, element.lastFocused_);
    assertTrue(items[2].row_.isActive());
    assertFalse(items[3].row_.isActive());

    MockInteractions.pressAndReleaseKeyOn(focused, 37, [], 'ArrowLeft');
    Polymer.dom.flush();
    focused = items[2].$$('#bookmark-star');
    assertEquals(focused, element.lastFocused_);
    assertTrue(items[2].row_.isActive());
    assertFalse(items[3].row_.isActive());

    MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    Polymer.dom.flush();
    focused = items[3].$.link;
    assertEquals(focused, element.lastFocused_);
    assertFalse(items[2].row_.isActive());
    assertTrue(items[3].row_.isActive());
  });
});
