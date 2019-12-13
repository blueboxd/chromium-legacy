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
    window.history.replaceState({}, '', '/');
    PolymerTest.clearBody();
    testService = new TestBrowserService();
    history.BrowserService.instance_ = testService;
    testService.setQueryResult({
      info: createHistoryInfo(),
      value: TEST_HISTORY_RESULTS,
    });

    app = document.createElement('history-app');
    document.body.appendChild(app);
    element = app.$.history;
    return Promise
        .all([
          testService.whenCalled('queryHistory'),
          history.ensureLazyLoaded(),
        ])
        .then(test_util.flushTasks);
  });

  test('list focus and keyboard nav', async () => {
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

  test('selection of all items using ctrl + a', async () => {
    const toolbar = app.$.toolbar;
    const field = toolbar.$['main-toolbar'].getSearchField();
    field.blur();
    assertFalse(field.showingSearch);

    const modifier = cr.isMac ? 'meta' : 'ctrl';
    let promise = test_util.eventToPromise('keydown', document);
    MockInteractions.pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    let keydownEvent = await promise;
    assertTrue(keydownEvent.defaultPrevented);

    assertDeepEquals(
        [true, true, true, true], element.historyData_.map(i => i.selected));

    // If everything is already selected, the same shortcut will trigger
    // cancelling selection.
    MockInteractions.pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    assertDeepEquals(
        [false, false, false, false],
        element.historyData_.map(i => i.selected));

    // If the search field is focused, the keyboard event should be handled by
    // the browser (which triggers selection of the text within the search
    // input).
    field.getSearchInput().focus();
    promise = test_util.eventToPromise('keydown', document);
    MockInteractions.pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    keydownEvent = await promise;
    assertFalse(keydownEvent.defaultPrevented);
    assertDeepEquals(
        [false, false, false, false],
        element.historyData_.map(i => i.selected));
  });
});
