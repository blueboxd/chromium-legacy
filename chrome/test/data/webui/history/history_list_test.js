// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService, ensureLazyLoaded} from 'chrome://history/history.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {createHistoryEntry, createHistoryInfo, polymerSelectAll, shiftClick, waitForEvent} from 'chrome://test/history/test_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

suite('<history-list>', function() {
  let app;
  let element;
  let toolbar;
  let testService;

  const TEST_HISTORY_RESULTS = [
    createHistoryEntry('2016-03-15', 'https://www.google.com'),
    createHistoryEntry('2016-03-14 10:00', 'https://www.example.com'),
    createHistoryEntry('2016-03-14 9:00', 'https://www.google.com'),
    createHistoryEntry('2016-03-13', 'https://en.wikipedia.org')
  ];
  TEST_HISTORY_RESULTS[2].starred = true;

  const ADDITIONAL_RESULTS = [
    createHistoryEntry('2016-03-13 10:00', 'https://en.wikipedia.org'),
    createHistoryEntry('2016-03-13 9:50', 'https://www.youtube.com'),
    createHistoryEntry('2016-03-11', 'https://www.google.com'),
    createHistoryEntry('2016-03-10', 'https://www.example.com')
  ];

  setup(function() {
    window.history.replaceState({}, '', '/');
    PolymerTest.clearBody();
    testService = new TestBrowserService();
    BrowserService.instance_ = testService;

    app = document.createElement('history-app');
  });

  /**
   * @param {!Array<!HistoryEntry>} queryResults The query results to initialize
   *     the page with.
   * @param {string=} query The query to use in the QueryInfo.
   * @return {!Promise} Promise that resolves when initialization is complete
   *     and the lazy loaded module has been loaded.
   */
  function finishSetup(queryResults, query) {
    testService.setQueryResult(
        {info: createHistoryInfo(query), value: queryResults});
    document.body.appendChild(app);

    element = app.$.history;
    toolbar = app.$.toolbar;
    app.queryState_.incremental = true;
    return Promise.all([
      testService.whenCalled('queryHistory'),
      ensureLazyLoaded(),
    ]);
  }

  test('deleting single item', function() {
    return finishSetup([createHistoryEntry('2015-01-01', 'http://example.com')])
        .then(flushTasks)
        .then(function() {
          assertEquals(element.historyData_.length, 1);
          flush();
          const items = polymerSelectAll(element, 'history-item');

          assertEquals(1, items.length);
          items[0].$.checkbox.click();
          assertDeepEquals([true], element.historyData_.map(i => i.selected));
          return flushTasks();
        })
        .then(function() {
          toolbar.deleteSelectedItems();
          return flushTasks();
        })
        .then(function() {
          const dialog = element.$.dialog.get();
          assertTrue(dialog.open);
          testService.resetResolver('queryHistory');
          element.$$('.action-button').click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(visits) {
          assertEquals(1, visits.length);
          assertEquals('http://example.com', visits[0].url);
          assertEquals('2015-01-01 UTC', visits[0].timestamps[0]);

          // The list should fire a query-history event which results in a
          // queryHistory call, since deleting the only item results in an
          // empty history list.
          return testService.whenCalled('queryHistory');
        });
  });

  test('cancelling selection of multiple items', function() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(flushTasks)
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');

          items[2].$.checkbox.click();
          items[3].$.checkbox.click();

          // Make sure that the array of data that determines whether or not an
          // item is selected is what we expect after selecting the two items.
          assertDeepEquals(
              [false, false, true, true],
              element.historyData_.map(i => i.selected));

          toolbar.clearSelectedItems();

          // Make sure that clearing the selection updates both the array and
          // the actual history-items affected.
          assertDeepEquals(
              [false, false, false, false],
              element.historyData_.map(i => i.selected));

          assertFalse(items[2].selected);
          assertFalse(items[3].selected);
        });
  });

  test('selection of multiple items using shift click', function() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(flushTasks)
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');

          items[1].$.checkbox.click();
          assertDeepEquals(
              [false, true, false, false],
              element.historyData_.map(i => i.selected));
          assertDeepEquals([1], Array.from(element.selectedItems).sort());

          // Shift-select to the last item.
          shiftClick(items[3].$.checkbox);
          assertDeepEquals(
              [false, true, true, true],
              element.historyData_.map(i => i.selected));
          assertDeepEquals([1, 2, 3], Array.from(element.selectedItems).sort());

          // Shift-select back to the first item.
          shiftClick(items[0].$.checkbox);
          assertDeepEquals(
              [true, true, true, true],
              element.historyData_.map(i => i.selected));
          assertDeepEquals(
              [0, 1, 2, 3], Array.from(element.selectedItems).sort());

          // Shift-deselect to the third item.
          shiftClick(items[2].$.checkbox);
          assertDeepEquals(
              [false, false, false, true],
              element.historyData_.map(i => i.selected));
          assertDeepEquals([3], Array.from(element.selectedItems).sort());

          // Select the second item.
          items[1].$.checkbox.click();
          assertDeepEquals(
              [false, true, false, true],
              element.historyData_.map(i => i.selected));
          assertDeepEquals([1, 3], Array.from(element.selectedItems).sort());

          // Shift-deselect to the last item.
          shiftClick(items[3].$.checkbox);
          assertDeepEquals(
              [false, false, false, false],
              element.historyData_.map(i => i.selected));
          assertDeepEquals([], Array.from(element.selectedItems).sort());

          // Shift-select back to the third item.
          shiftClick(items[2].$.checkbox);
          assertDeepEquals(
              [false, false, true, true],
              element.historyData_.map(i => i.selected));
          assertDeepEquals([2, 3], Array.from(element.selectedItems).sort());

          // Remove selected items.
          element.removeItemsByIndex_(Array.from(element.selectedItems));
          assertDeepEquals(
              ['https://www.google.com', 'https://www.example.com'],
              element.historyData_.map(i => i.title));
        });
  });

  // See http://crbug.com/845802.
  test('disabling ctrl + a command on syncedTabs page', function() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          app.selectedPage_ = 'syncedTabs';
          return flushTasks();
        })
        .then(function() {
          const field = toolbar.$['main-toolbar'].getSearchField();
          field.blur();
          assertFalse(field.showingSearch);

          const modifier = isMac ? 'meta' : 'ctrl';
          pressAndReleaseKeyOn(
              document.body, 65, modifier, 'a');

          assertDeepEquals(
              [false, false, false, false],
              element.historyData_.map(i => i.selected));
        });
  });

  test('setting first and last items', function() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(flushTasks)
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');
          assertTrue(items[0].isCardStart);
          assertTrue(items[0].isCardEnd);
          assertFalse(items[1].isCardEnd);
          assertFalse(items[2].isCardStart);
          assertTrue(items[2].isCardEnd);
          assertTrue(items[3].isCardStart);
          assertTrue(items[3].isCardEnd);
        });
  });

  function loadWithAdditionalResults() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          testService.resetResolver('queryHistory');
          testService.setQueryResult(
              {info: createHistoryInfo(), value: ADDITIONAL_RESULTS});
          element.fire('query-history', true);
          return testService.whenCalled('queryHistoryContinuation');
        })
        .then(flushTasks);
  }

  test('updating history results', function() {
    return loadWithAdditionalResults()
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');
          assertTrue(items[3].isCardStart);
          assertTrue(items[5].isCardEnd);

          assertTrue(items[6].isCardStart);
          assertTrue(items[6].isCardEnd);

          assertTrue(items[7].isCardStart);
          assertTrue(items[7].isCardEnd);
        });
  });

  test('deleting multiple items from view', function() {
    return loadWithAdditionalResults()
        .then(function() {
          element.removeItemsByIndex_([2, 5, 7]);
          return flushTasks();
        })
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');

          assertEquals(element.historyData_.length, 5);
          assertEquals(element.historyData_[0].dateRelativeDay, '2016-03-15');
          assertEquals(element.historyData_[2].dateRelativeDay, '2016-03-13');
          assertEquals(element.historyData_[4].dateRelativeDay, '2016-03-11');

          // Checks that the first and last items have been reset correctly.
          assertTrue(items[2].isCardStart);
          assertTrue(items[3].isCardEnd);
          assertTrue(items[4].isCardStart);
          assertTrue(items[4].isCardEnd);
        });
  });

  test('search results display with correct item title', function() {
    return finishSetup(
               [createHistoryEntry('2016-03-15', 'https://www.google.com')])
        .then(function() {
          element.searchedTerm = 'Google';
          flushTasks();
        })
        .then(function() {
          flush();
          const item = element.$$('history-item');
          assertTrue(item.isCardStart);
          const heading = item.$$('#date-accessed').textContent;
          const title = item.$.link;

          // Check that the card title displays the search term somewhere.
          const index = heading.indexOf('Google');
          assertTrue(index != -1);

          // Check that the search term is bolded correctly in the history-item.
          assertGT(title.children[1].innerHTML.indexOf('<b>google</b>'), -1);
        });
  });

  test('correct display message when no history available', function() {
    return finishSetup([])
        .then(flushTasks)
        .then(function() {
          assertFalse(element.$['no-results'].hidden);
          assertNotEquals('', element.$['no-results'].textContent.trim());
          assertTrue(element.$['infinite-list'].hidden);

          testService.setQueryResult(
              {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS});
          element.fire('query-history', false);
          return testService.whenCalled('queryHistory');
        })
        .then(flushTasks)
        .then(function() {
          assertTrue(element.$['no-results'].hidden);
          assertFalse(element.$['infinite-list'].hidden);
        });
  });

  test('more from this site sends and sets correct data', function() {
    let items;
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(flushTasks)
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          testService.resetResolver('queryHistory');
          testService.setQueryResult({
            info: createHistoryInfo('www.google.com'),
            value: TEST_HISTORY_RESULTS,
          });
          items = polymerSelectAll(element, 'history-item');
          items[0].$['menu-button'].click();
          element.$.sharedMenu.get();
          element.$$('#menuMoreButton').click();
          return testService.whenCalled('queryHistory');
        })
        .then(function(query) {
          assertEquals('www.google.com', query);
          return flushTasks();
        })
        .then(function() {
          assertEquals(
              'www.google.com',
              toolbar.$['main-toolbar'].getSearchField().getValue());

          element.$.sharedMenu.get().close();
          items[0].$['menu-button'].click();
          assertTrue(element.$$('#menuMoreButton').hidden);

          element.$.sharedMenu.get().close();
          items[1].$['menu-button'].click();
          assertFalse(element.$$('#menuMoreButton').hidden);
        });
  });

  // TODO(calamity): Reenable this test after fixing flakiness.
  // See http://crbug.com/640862.
  test.skip('scrolling history list causes toolbar shadow to appear', () => {
    const loadMoreResults = function(numReloads) {
      testService.resetResolver('queryHistory');
      testService.setQueryResult(
          {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS});
      element.fire('query-history', true);
      const promise = testService.whenCalled('queryHistoryContinuation');
      return numReloads === 1 ? promise :
                                promise.then(loadMoreResults(numReloads - 1));
    };
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(loadMoreResults(9))
        .then(flushTasks)
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(() => {
          assertFalse(app.toolbarShadow_);
          element.$['infinite-list'].scrollToIndex(20);
          return waitForEvent(app, 'toolbar-shadow_-changed');
        })
        .then(() => {
          assertTrue(app.toolbarShadow_);
          element.$['infinite-list'].scrollToIndex(0);
          return waitForEvent(app, 'toolbar-shadow_-changed');
        })
        .then(() => {
          assertFalse(app.toolbarShadow_);
        });
  });

  test('changing search deselects items', function() {
    return finishSetup(
               [createHistoryEntry('2016-06-9', 'https://www.example.com')],
               'ex')
        .then(flushTasks(20))
        .then(function() {
          flush();
          const item = element.$$('history-item');
          item.$.checkbox.click();

          assertEquals(1, toolbar.count);
          app.queryState_.incremental = false;

          testService.resetResolver('queryHistory');
          testService.setQueryResult({
            info: createHistoryInfo('ample'),
            value: [createHistoryEntry('2016-06-9', 'https://www.example.com')],
          });
          element.fire('query-history', false);
          return testService.whenCalled('queryHistory');
        })
        .then(function() {
          assertEquals(0, toolbar.count);
        });
  });

  test('delete items end to end', function() {
    let dialog;
    return loadWithAdditionalResults()
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          dialog = element.$.dialog.get();
          return flushTasks();
        })
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');

          items[2].$.checkbox.click();
          items[5].$.checkbox.click();
          items[7].$.checkbox.click();

          return flushTasks();
        })
        .then(function() {
          toolbar.deleteSelectedItems();
          return flushTasks();
        })
        .then(function() {
          testService.resetResolver('removeVisits');
          // Confirmation dialog should appear.
          assertTrue(dialog.open);
          element.$$('.action-button').click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(visits) {
          assertEquals(3, visits.length);
          assertEquals(TEST_HISTORY_RESULTS[2].url, visits[0].url);
          assertEquals(
              TEST_HISTORY_RESULTS[2].allTimestamps[0],
              visits[0].timestamps[0]);
          assertEquals(ADDITIONAL_RESULTS[1].url, visits[1].url);
          assertEquals(
              ADDITIONAL_RESULTS[1].allTimestamps[0], visits[1].timestamps[0]);
          assertEquals(ADDITIONAL_RESULTS[3].url, visits[2].url);
          assertEquals(
              ADDITIONAL_RESULTS[3].allTimestamps[0], visits[2].timestamps[0]);
          return flushTasks();
        })
        .then(flushTasks)
        .then(function() {
          assertEquals(5, element.historyData_.length);
          assertEquals(element.historyData_[0].dateRelativeDay, '2016-03-15');
          assertEquals(element.historyData_[2].dateRelativeDay, '2016-03-13');
          assertEquals(element.historyData_[4].dateRelativeDay, '2016-03-11');
          assertFalse(dialog.open);

          flush();
          // Ensure the UI is correctly updated.
          const items = polymerSelectAll(element, 'history-item');

          assertEquals('https://www.google.com', items[0].item.title);
          assertEquals('https://www.example.com', items[1].item.title);
          assertEquals('https://en.wikipedia.org', items[2].item.title);
          assertEquals('https://en.wikipedia.org', items[3].item.title);
          assertEquals('https://www.google.com', items[4].item.title);
        });
  });

  test('delete via menu button', function() {
    let items;
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(flushTasks)
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          items = polymerSelectAll(element, 'history-item');
          items[1].$.checkbox.click();
          items[3].$.checkbox.click();
          items[1].$['menu-button'].click();
          element.$.sharedMenu.get();
          element.$$('#menuRemoveButton').click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(visits) {
          assertEquals(1, visits.length);
          assertEquals(TEST_HISTORY_RESULTS[1].url, visits[0].url);
          assertEquals(
              TEST_HISTORY_RESULTS[1].allTimestamps[0],
              visits[0].timestamps[0]);
          return flushTasks();
        })
        .then(flushTasks)
        .then(function() {
          assertDeepEquals(
              [
                'https://www.google.com',
                'https://www.google.com',
                'https://en.wikipedia.org',
              ],
              element.historyData_.map(item => item.title));

          // Deletion should deselect all.
          assertDeepEquals(
              [false, false, false],
              Array.from(items).slice(0, 3).map(i => i.selected));
        });
  });

  test('delete disabled while pending', function() {
    let items;
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          testService.delayDelete();
          return flushTasks();
        })
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          items = polymerSelectAll(element, 'history-item');
          items[1].$.checkbox.click();
          items[2].$.checkbox.click();
          items[1].$['menu-button'].click();
          element.$.sharedMenu.get();
          element.$$('#menuRemoveButton').click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(visits) {
          assertEquals(1, visits.length);
          assertEquals(TEST_HISTORY_RESULTS[1].url, visits[0].url);
          assertEquals(
              TEST_HISTORY_RESULTS[1].allTimestamps[0],
              visits[0].timestamps[0]);

          // Deletion is still happening. Verify that menu button and toolbar
          // are disabled.
          assertTrue(element.$$('#menuRemoveButton').disabled);
          assertEquals(2, toolbar.count);
          assertTrue(toolbar.$$('cr-toolbar-selection-overlay').deleteDisabled);

          // Key event should be ignored.
          assertEquals(1, testService.getCallCount('removeVisits'));
          pressAndReleaseKeyOn(
              document.body, 46, '', 'Delete');

          return flushTasks();
        })
        .then(function() {
          assertEquals(1, testService.getCallCount('removeVisits'));
          testService.finishRemoveVisits();
          return flushTasks();
        })
        .then(flushTasks)
        .then(function() {
          // Reselect some items.
          items = polymerSelectAll(element, 'history-item');
          items[1].$.checkbox.click();
          items[2].$.checkbox.click();

          // Check that delete option is re-enabled.
          assertEquals(2, toolbar.count);
          assertFalse(
              toolbar.$$('cr-toolbar-selection-overlay').deleteDisabled);

          // Menu button should also be re-enabled.
          items[1].$['menu-button'].click();
          element.$.sharedMenu.get();
          assertFalse(element.$$('#menuRemoveButton').disabled);
        });
  });

  test('deleting items using shortcuts', function() {
    let dialog;
    let items;
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          dialog = element.$.dialog.get();
          flushTasks();
        })
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          items = polymerSelectAll(element, 'history-item');

          // Dialog should not appear when there is no item selected.
          pressAndReleaseKeyOn(
              document.body, 46, '', 'Delete');
          return flushTasks();
        })
        .then(function() {
          assertFalse(dialog.open);

          items[1].$.checkbox.click();
          items[2].$.checkbox.click();

          assertEquals(2, toolbar.count);

          pressAndReleaseKeyOn(
              document.body, 46, '', 'Delete');
          return flushTasks();
        })
        .then(function() {
          assertTrue(dialog.open);
          element.$$('.cancel-button').click();
          assertFalse(dialog.open);

          pressAndReleaseKeyOn(
              document.body, 8, '', 'Backspace');
          return flushTasks();
        })
        .then(function() {
          assertTrue(dialog.open);
          element.$$('.action-button').click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(toRemove) {
          assertEquals('https://www.example.com', toRemove[0].url);
          assertEquals('https://www.google.com', toRemove[1].url);
          assertEquals('2016-03-14 10:00 UTC', toRemove[0].timestamps[0]);
          assertEquals('2016-03-14 9:00 UTC', toRemove[1].timestamps[0]);
        });
  });

  test('delete dialog closed on back navigation', function() {
    // Ensure that state changes are always mirrored to the URL.
    return finishSetup([])
        .then(function() {
          testService.resetResolver('queryHistory');
          app.$$('history-router').$$('iron-location').dwellTime = 0;

          testService.setQueryResult({
            info: createHistoryInfo('something else'),
            value: TEST_HISTORY_RESULTS,
          });

          // Navigate from chrome://history/ to
          // chrome://history/?q=something else.
          app.fire('change-query', {search: 'something else'});
          return testService.whenCalled('queryHistory');
        })
        .then(function() {
          testService.resetResolver('queryHistory');
          testService.setQueryResult({
            info: createHistoryInfo('something else'),
            value: ADDITIONAL_RESULTS
          });
          element.fire('query-history', true);
          return testService.whenCalled('queryHistoryContinuation');
        })
        .then(flushTasks)
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');

          items[2].$.checkbox.click();
          return flushTasks();
        })
        .then(function() {
          toolbar.deleteSelectedItems();
          return flushTasks();
        })
        .then(function() {
          // Confirmation dialog should appear.
          assertTrue(element.$.dialog.getIfExists().open);
          // Navigate back to chrome://history.
          window.history.back();

          return waitForEvent(window, 'popstate');
        })
        .then(flushTasks)
        .then(function() {
          assertFalse(element.$.dialog.getIfExists().open);
        });
  });

  test('clicking file:// url sends message to chrome', function() {
    const fileURL = 'file:///home/myfile';
    return finishSetup([createHistoryEntry('2016-03-15', fileURL)])
        .then(flushTasks)
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');
          items[0].$.link.click();
          return testService.whenCalled('navigateToUrl');
        })
        .then(function(url) {
          assertEquals(fileURL, url);
        });
  });

  test('deleteHistory results in query-history event', function() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          testService.resetResolver('queryHistory');
          cr.webUIListenerCallback('history-deleted');
        })
        .then(flushTasks)
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = polymerSelectAll(element, 'history-item');

          items[2].$.checkbox.click();
          items[3].$.checkbox.click();

          testService.resetResolver('queryHistory');
          cr.webUIListenerCallback('history-deleted');
          flushTasks();
          assertEquals(0, testService.getCallCount('queryHistory'));
        });
  });

  teardown(function() {
    app.fire('change-query', {search: ''});
  });
});
