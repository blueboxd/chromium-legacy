// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('history-toolbar', function() {
  let app;
  let element;
  let toolbar;
  let testService;
  const TEST_HISTORY_RESULTS =
      [createHistoryEntry('2016-03-15', 'https://google.com')];

  setup(function() {
    PolymerTest.clearBody();
    testService = new TestBrowserService();
    history.BrowserService.instance_ = testService;

    app = document.createElement('history-app');
    document.body.appendChild(app);
    element = app.$.history;
    toolbar = app.$.toolbar;
    return test_util.flushTasks();
  });

  test('selecting checkbox causes toolbar to change', function() {
    element.addNewResults(TEST_HISTORY_RESULTS);

    return test_util.flushTasks().then(function() {
      const item = element.$$('history-item');
      MockInteractions.tap(item.$.checkbox);

      // Ensure that when an item is selected that the count held by the
      // toolbar increases.
      assertEquals(1, toolbar.count);
      // Ensure that the toolbar boolean states that at least one item is
      // selected.
      assertTrue(toolbar.itemsSelected_);

      MockInteractions.tap(item.$.checkbox);

      // Ensure that when an item is deselected the count held by the
      // toolbar decreases.
      assertEquals(0, toolbar.count);
      // Ensure that the toolbar boolean states that no items are selected.
      assertFalse(toolbar.itemsSelected_);
    });
  });

  test('search term gathered correctly from toolbar', function() {
    app.queryState_.queryingDisabled = false;
    toolbar.$$('cr-toolbar').fire('search-changed', 'Test');
    return testService.whenCalled('queryHistory').then(query => {
      assertEquals('Test', query);
      app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
    });
  });

  test('spinner is active on search', function() {
    app.queryState_.queryingDisabled = false;
    toolbar.$$('cr-toolbar').fire('search-changed', 'Test2');
    return testService.whenCalled('queryHistory')
        .then(test_util.flushTasks)
        .then(() => {
          assertTrue(toolbar.spinnerActive);
          app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
          assertFalse(toolbar.spinnerActive);
        });
  });

  test('menu promo hides when drawer is opened', function() {
    app.showMenuPromo_ = true;
    app.hasDrawer_ = true;
    Polymer.dom.flush();
    MockInteractions.tap(toolbar.$['main-toolbar'].$$('#menuButton'));
    assertFalse(app.showMenuPromo_);
  });
});
