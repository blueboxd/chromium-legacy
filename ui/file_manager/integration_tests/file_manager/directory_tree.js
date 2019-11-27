// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
(() => {
  /**
   * Tests that the directory tree can be vertically scrolled.
   */
  testcase.directoryTreeVerticalScroll = async () => {
    // Add directory entries to overflow the directory tree container.
    const folders = [ENTRIES.photos];
    for (let i = 0; i < 30; i++) {
      folders.push(new TestEntryInfo({
        type: EntryType.DIRECTORY,
        targetPath: '' + i,
        lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
        nameText: '' + i,
        sizeText: '--',
        typeText: 'Folder'
      }));
    }

    // Open FilesApp on Downloads. Expand the directory tree view of Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, folders, []);
    await expandRoot(appId, TREEITEM_DOWNLOADS);

    // Get the directory tree and verify it is not scrolled.
    const directoryTree = '#directory-tree';
    const original = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollTop']);
    chrome.test.assertTrue(original.scrollTop === 0);

    // Scroll the directory tree down (vertical scroll).
    await remoteCall.callRemoteTestUtil(
        'setScrollTop', appId, [directoryTree, 100]);

    // Check: the directory tree should be scrolled.
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollTop']);
    chrome.test.assertTrue(scrolled.scrollTop === 100, 'Tree should scroll');
  };

  /**
   * Tests that the directory tree does not horizontally scroll.
   */
  testcase.directoryTreeHorizontalScroll = async () => {
    // Open FilesApp on Downloads. Expand the navigation list view of Downloads.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
    await expandRoot(appId, TREEITEM_DOWNLOADS);

    // Get the directory tree and verify it is not scrolled.
    const directoryTree = '#directory-tree';
    const original = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    chrome.test.assertTrue(original.scrollLeft === 0);

    // Shrink the navigation list tree to 50px.
    const navigationList = '.dialog-navigation-list';
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '50px'}]);

    // Scroll the directory tree left (horizontal scroll).
    await remoteCall.callRemoteTestUtil(
        'setScrollLeft', appId, [directoryTree, 100]);

    // Check; the directory tree should not be scrolled.
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    chrome.test.assertTrue(scrolled.scrollLeft === 0, 'Tree should not scroll');
  };

  /**
   * Tests that the directory tree does not horizontally scroll when using the
   * keyboard right-arrow to navigate nested folders.
   */
  testcase.directoryTreeKeyboardHorizontalScroll = async () => {
    /**
     * Helper to create a test folder from a full |path|.
     * @param {string} path The folder path.
     * @return {TestEntryInfo}
     */
    function createTestFolder(path) {
      const name = path.split('/').pop();
      return new TestEntryInfo({
        targetPath: path,
        nameText: name,
        type: EntryType.DIRECTORY,
        lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
        sizeText: '--',
        typeText: 'Folder',
      });
    }

    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = [];
    for (let path = 'nested-folder0', i = 0; i < 8; ++i) {
      nestedFolderTestEntries.push(createTestFolder(path));
      path += `/nested-folder${i + 1}`;
    }

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Verify the directory tree is not horizontally scrolled.
    const directoryTree = '#directory-tree';
    const original = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    chrome.test.assertTrue(original.scrollLeft === 0);

    // Shrink the tree to 150px, enough to elide the deep folder names.
    const navigationList = '.dialog-navigation-list';
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '150px'}]);

    // Select Downloads folder.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'selectFolderInTree', appId, ['Downloads']));

    // Keyboard expand Downloads > nested-folder0 > nested-folder1 > ...
    for (let i = 0; i < nestedFolderTestEntries.length; ++i) {
      chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
          'expandSelectedFolderInTree', appId, []));
      chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
          'expandSelectedFolderInTree', appId, []));
      chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
          'requestAnimationFrame', appId, []));
    }

    // Wait until the directory tree has selected the last folder.
    await remoteCall.waitForElement(
        appId, '.tree-item[selected][entry-label="nested-folder7"]');

    // Check: the directory tree should not be horizontally scrolled.
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    const noScrollLeft = scrolled.scrollLeft === 0;
    chrome.test.assertTrue(noScrollLeft, 'Tree should not scroll left');
  };
})();
