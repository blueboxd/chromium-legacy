// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<history-toolbar>', function() {
  let app;
  let toolbar;

  setup(function() {
    window.resultsRendered = false;
    app = replaceApp();

    toolbar = app.$['toolbar'];
  });

  test('search bar is focused on load in wide mode', async () => {
    toolbar.$['main-toolbar'].narrow = false;

    historyResult(createHistoryInfo(), []);
    await test_util.flushTasks();

    // Ensure the search bar is focused on load.
    assertTrue(
        app.$.toolbar.$['main-toolbar'].getSearchField().isSearchFocused());
  });

  test('search bar is not focused on load in narrow mode', async () => {
    toolbar.$['main-toolbar'].narrow = true;

    historyResult(createHistoryInfo(), []);
    await test_util.flushTasks();
    // Ensure the search bar is focused on load.
    assertFalse($('history-app')
                    .$.toolbar.$['main-toolbar']
                    .getSearchField()
                    .isSearchFocused());
  });

  test('shortcuts to open search field', function() {
    const field = toolbar.$['main-toolbar'].getSearchField();
    field.blur();
    assertFalse(field.showingSearch);

    const modifier = cr.isMac ? 'meta' : 'ctrl';
    MockInteractions.pressAndReleaseKeyOn(document.body, 70, modifier, 'f');
    assertTrue(field.showingSearch);
    assertEquals(field.$.searchInput, field.root.activeElement);

    MockInteractions.pressAndReleaseKeyOn(
        field.$.searchInput, 27, '', 'Escape');
    assertFalse(field.showingSearch, 'Pressing escape closes field.');
    assertNotEquals(field.$.searchInput, field.root.activeElement);
  });
});
