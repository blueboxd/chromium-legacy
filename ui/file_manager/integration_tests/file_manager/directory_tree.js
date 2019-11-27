// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Tests that the directory tree can be vertically scrolled.
   */
  testcase.directoryTreeVerticalScroll = async () => {
    const folders = [ENTRIES.photos];

    // Add enough test entries to overflow the #directory-tree container.
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

    // Open FilesApp on Downloads and expand the tree view of Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, folders, []);
    await expandRoot(appId, TREEITEM_DOWNLOADS);

    // Verify the directory tree is not vertically scrolled.
    const directoryTree = '#directory-tree';
    const original = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollTop']);
    chrome.test.assertTrue(original.scrollTop === 0);

    // Scroll the directory tree down (vertical scroll).
    await remoteCall.callRemoteTestUtil(
        'setScrollTop', appId, [directoryTree, 100]);

    // Check: the directory tree should be vertically scrolled.
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollTop']);
    const scrolledDown = scrolled.scrollTop === 100;
    chrome.test.assertTrue(scrolledDown, 'Tree should scroll down');
  };

  /**
   * Tests that the directory tree does not horizontally scroll.
   */
  testcase.directoryTreeHorizontalScroll = async () => {
    // Open FilesApp on Downloads and expand the tree view of Downloads.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
    await expandRoot(appId, TREEITEM_DOWNLOADS);

    // Verify the directory tree is not horizontally scrolled.
    const directoryTree = '#directory-tree';
    const original = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    chrome.test.assertTrue(original.scrollLeft === 0);

    // Shrink the tree to 50px. TODO(files-ng): consider using 150px?
    const navigationList = '.dialog-navigation-list';
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '50px'}]);

    // Scroll the directory tree left (horizontal scroll).
    await remoteCall.callRemoteTestUtil(
        'setScrollLeft', appId, [directoryTree, 100]);

    // Check: the directory tree should not be horizontally scrolled.
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    const noScrollLeft = scrolled.scrollLeft === 0;
    chrome.test.assertTrue(noScrollLeft, 'Tree should not scroll left');
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
    for (let path = 'nested-folder0', i = 0; i < 6; ++i) {
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

    // Focus the tree and prepare to send it ArrowRight keys in pairs.
    await remoteCall.callRemoteTestUtil('focus', appId, ['#directory-tree']);
    const key = [directoryTree, 'ArrowRight', false, false, false];

    // Emit keyboard events to select, then expand, the nested folders.
    for (let i = 0; i < nestedFolderTestEntries.length; ++i) {
      chrome.test.assertTrue(
          await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));
      chrome.test.assertTrue(
          await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));
      chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
          'requestAnimationFrame', appId, []));
    }

    // Wait until the directory tree has selected the last folder.
    await remoteCall.waitForElement(
        appId, '.tree-item[selected][entry-label="nested-folder5"]');

    // Check: the directory tree should not be horizontally scrolled.
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    const noScrollLeft = scrolled.scrollLeft === 0;
    chrome.test.assertTrue(noScrollLeft, 'Tree should not scroll left');
  };
})();
