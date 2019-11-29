// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
(() => {
  /**
   * Expected files shown in the search results for 'hello'
   *
   * @type {Array<TestEntryInfo>}
   * @const
   */
  const SEARCH_RESULTS_ENTRY_SET = [
    ENTRIES.hello,
  ];

  /**
   * Tests searching inside Downloads with results.
   */
  testcase.searchDownloadsWithResults = async () => {
    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

    // Click on the search button to display the search box.
    await remoteCall.waitAndClickElement(appId, '#search-button');

    // Focus the search box.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box [type="search"]', 'focus']));

    // Input a text.
    await remoteCall.callRemoteTestUtil(
        'inputText', appId, ['#search-box [type="search"]', 'hello']);

    // Notify the element of the input.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box [type="search"]', 'input']));

    // Wait file list to display the search result.
    await remoteCall.waitForFiles(
        appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

    // Check that a11y message for results has been issued.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
    chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
    chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);

    return appId;
  };

  /**
   * Tests searching inside Downloads without results.
   */
  testcase.searchDownloadsWithNoResults = async () => {
    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

    // Focus the search box.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box [type="search"]', 'focus']));

    // Input a text.
    await remoteCall.callRemoteTestUtil(
        'inputText', appId, ['#search-box [type="search"]', 'INVALID TERM']);

    // Notify the element of the input.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box [type="search"]', 'input']));

    // Wait file list to display no results.
    await remoteCall.waitForFiles(appId, []);

    // Check that a11y message for no results has been issued.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
    chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
    chrome.test.assertEq(
        'There are no results for INVALID TERM.', a11yMessages[0]);
  };

  /**
   * Tests that clearing the search box announces the A11y.
   */
  testcase.searchDownloadsClearSearch = async () => {
    // Perform a normal search, to be able to clear the search box.
    const appId = await testcase.searchDownloadsWithResults();

    // Click on the clear search button.
    await remoteCall.waitAndClickElement(appId, '#search-box .clear');

    // Wait for fil list to display all files.
    await remoteCall.waitForFiles(
        appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));

    // Check that a11y message for clearing the search term has been issued.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
    chrome.test.assertEq(2, a11yMessages.length, 'Missing a11y message');
    chrome.test.assertEq(
        'Search text cleared, showing all files and folders.', a11yMessages[1]);
  };

  /**
   * Tests that clearing the search box with keydown crbug.com/910068.
   */
  testcase.searchDownloadsClearSearchKeyDown = async () => {
    // Perform a normal search, to be able to clear the search box.
    const appId = await testcase.searchDownloadsWithResults();

    const clearButton = '#search-box .clear';
    // Wait for clear button.
    await remoteCall.waitForElement(appId, clearButton);

    // Send a enter key to the clear button.
    const enterKey = [clearButton, 'Enter', false, false, false];
    await remoteCall.fakeKeyDown(appId, ...enterKey);

    // Check: Search input field is empty.
    const searchInput =
        await remoteCall.waitForElement(appId, '#search-box [type="search"]');
    chrome.test.assertEq('', searchInput.value);
  };

  /**
   * Tests that the search text entry box stays expanded until the end of user
   * interaction.
   */
  testcase.searchHidingTextEntryField = async () => {
    const entry = ENTRIES.hello;

    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

    // Select an entry in the file list.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'selectFile', appId, [entry.nameText]));

    // Focus the toolbar search button.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-button', 'click']));

    // Verify the toolbar search text entry box is enabled.
    let textInputElement =
        await remoteCall.waitForElement(appId, ['#search-box cr-input']);
    chrome.test.assertEq('false', textInputElement.attributes['aria-disabled']);

    // Send a 'mousedown' to the toolbar 'delete' button.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#delete-button', 'mousedown']));

    // Verify the toolbar search text entry is still enabled.
    textInputElement =
        await remoteCall.waitForElement(appId, ['#search-box cr-input']);
    chrome.test.assertEq('false', textInputElement.attributes['aria-disabled']);

    // Send a 'mouseup' to the toolbar 'delete' button.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#delete-button', 'mouseup']));

    // Verify the toolbar search text entry is still enabled.
    textInputElement =
        await remoteCall.waitForElement(appId, ['#search-box cr-input']);
    chrome.test.assertEq('false', textInputElement.attributes['aria-disabled']);
  };
})();
