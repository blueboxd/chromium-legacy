// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the OS settings search box. */

suite('OSSettingsSearchBox', () => {
  /** @type {?OsToolbar} */
  let toolbar;

  /** @type {?OsSettingsSearchBox} */
  let searchBox;

  /** @type {?CrSearchFieldElement} */
  let field;

  /** @type {?IronDropdownElement} */
  let dropDown;

  /** @type {?IronListElement} */
  let resultList;

  /** @type {*} */
  let settingsSearchHandler;

  /** @type {?chromeos.settings.mojom.UserActionRecorderInterface} */
  let userActionRecorder;

  /** @param {string} term */
  async function simulateSearch(term) {
    field.$.searchInput.value = term;
    field.onSearchTermInput();
    field.onSearchTermSearch();
    await settingsSearchHandler.search;
    Polymer.dom.flush();
  }

  /**
   * @param {string} resultText Exact string of the result to be displayed.
   * @return {!chromeos.settings.mojom.SearchResult} A search result.
   */
  function fakeResult(resultText) {
    return /** @type {!mojom.SearchResult} */ ({
      resultText: {
        data: Array.from(resultText, c => c.charCodeAt()),
      },
    });
  }

  setup(function() {
    toolbar = document.querySelector('os-settings-ui').$$('os-toolbar');
    assertTrue(!!toolbar);
    searchBox = toolbar.$$('os-settings-search-box');
    assertTrue(!!searchBox);
    field = searchBox.$$('cr-toolbar-search-field');
    assertTrue(!!field);
    dropDown = searchBox.$$('iron-dropdown');
    assertTrue(!!dropDown);
    resultList = searchBox.$$('iron-list');
    assertTrue(!!resultList);

    settingsSearchHandler = new settings.FakeSettingsSearchHandler();
    searchBox.setSearchHandlerForTesting(settingsSearchHandler);

    userActionRecorder = new settings.FakeUserActionRecorder();
    settings.setUserActionRecorderForTesting(userActionRecorder);
  });

  teardown(async () => {
    // Clear search field for next test.
    await simulateSearch('');
    settings.setUserActionRecorderForTesting(null);
    searchBox.setSearchHandlerForTesting(undefined);
  });

  test('User action search event', async () => {
    settingsSearchHandler.setFakeResults([]);

    assertEquals(userActionRecorder.searchCount, 0);
    await simulateSearch('query');
    assertEquals(userActionRecorder.searchCount, 1);
  });

  test('Dropdown opens correctly when results are fetched', async () => {
    // Closed dropdown if no results are returned.
    settingsSearchHandler.setFakeResults([]);
    assertFalse(dropDown.opened);
    await simulateSearch('query 1');
    assertFalse(dropDown.opened);
    assertEquals(userActionRecorder.searchCount, 1);

    // Open dropdown if results are returned.
    settingsSearchHandler.setFakeResults([fakeResult('result')]);
    await simulateSearch('query 2');
    assertTrue(dropDown.opened);
  });

  test('Restore previous existing search results', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('result 1')]);
    await simulateSearch('query');
    assertTrue(dropDown.opened);
    const resultRow = resultList.items[0];

    // Child blur elements except field should not trigger closing of dropdown.
    resultList.blur();
    assertTrue(dropDown.opened);
    dropDown.blur();
    assertTrue(dropDown.opened);

    // User clicks outside the search box, closing the dropdown.
    searchBox.blur();
    assertFalse(dropDown.opened);

    // User clicks on input, restoring old results and opening dropdown.
    field.$.searchInput.focus();
    assertEquals('query', field.$.searchInput.value);
    assertTrue(dropDown.opened);

    // The same result row exists.
    assertEquals(resultRow, resultList.items[0]);

    // Search field is blurred, closing the dropdown.
    field.$.searchInput.blur();
    assertFalse(dropDown.opened);

    // User clicks on input, restoring old results and opening dropdown.
    field.$.searchInput.focus();
    assertEquals('query', field.$.searchInput.value);
    assertTrue(dropDown.opened);

    // The same result row exists.
    assertEquals(resultRow, resultList.items[0]);
  });

  test('Search result rows are selected correctly', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('a'), fakeResult('b')]);
    await simulateSearch('query');
    assertTrue(dropDown.opened);
    assertEquals(resultList.items.length, 2);

    // The first row should be selected when results are fetched.
    assertEquals(resultList.selectedItem, resultList.items[0]);

    // Test ArrowUp and ArrowDown interaction with selecting.
    const arrowUpEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowUp', keyCode: 38});
    const arrowDownEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowDown', keyCode: 40});

    // ArrowDown event should select next row.
    searchBox.dispatchEvent(arrowDownEvent);
    assertEquals(resultList.selectedItem, resultList.items[1]);

    // If last row selected, ArrowDown brings select back to first row.
    searchBox.dispatchEvent(arrowDownEvent);
    assertEquals(resultList.selectedItem, resultList.items[0]);

    // If first row selected, ArrowUp brings select back to last row.
    searchBox.dispatchEvent(arrowUpEvent);
    assertEquals(resultList.selectedItem, resultList.items[1]);

    // ArrowUp should bring select previous row.
    searchBox.dispatchEvent(arrowUpEvent);
    assertEquals(resultList.selectedItem, resultList.items[0]);

    // Test that ArrowLeft and ArrowRight do nothing.
    const arrowLeftEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowLeft', keyCode: 37});
    const arrowRightEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowRight', keyCode: 39});

    // No change on ArrowLeft
    searchBox.dispatchEvent(arrowLeftEvent);
    assertEquals(resultList.selectedItem, resultList.items[0]);

    // No change on ArrowRight
    searchBox.dispatchEvent(arrowRightEvent);
    assertEquals(resultList.selectedItem, resultList.items[0]);
  });
});
