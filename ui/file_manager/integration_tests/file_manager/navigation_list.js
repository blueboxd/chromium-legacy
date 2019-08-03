// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
(() => {
  /**
   * Tests that the directory tree area can be vertically scrolled.
   */
  testcase.navigationListVerticalScroll = async () => {
    // Add directory entries to overflow the navigation list container.
    let folders = [ENTRIES.photos];
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

    // Open FilesApp on Downloads. Expand the navigation list view of Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, folders, []);
    await expandRoot(appId, TREEITEM_DOWNLOADS);

    // Get the navigationList and verify it is not scrolled.
    const navigationList = '.dialog-navigation-list';
    const originalList = await remoteCall.waitForElementStyles(
        appId, navigationList, ['scrollTop']);
    chrome.test.assertTrue(originalList.scrollTop === 0);

    // Scroll the navigation list down (vertical scroll).
    await remoteCall.callRemoteTestUtil(
        'setScrollTop', appId, [navigationList, 100]);

    // Check: the navigation list should be scrolled.
    const newList = await remoteCall.waitForElementStyles(
        appId, navigationList, ['scrollTop']);
    chrome.test.assertTrue(newList.scrollTop > 0);
  };

  /**
   * Tests that the directory tree area can be horizontally scrolled.
   * TODO(crbug.com/966807): Clean up test case to match the comment style and
   * organization of navigationListVerticalScroll.
   * Remove unnecessary check that originalWidth > 0.
   */
  testcase.navigationListHorizontalScroll = async () => {
    // Open FilesApp on Downloads. Expand the navigation list view of Downloads.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
    await expandRoot(appId, TREEITEM_DOWNLOADS);

    // Get the navigationList width property and make sure it has non-zero size.
    const navigationList = '.dialog-navigation-list';
    const originalList =
        await remoteCall.waitForElementStyles(appId, navigationList, ['width']);
    const originalWidth = Number(originalList.styles['width'].match(/[0-9]*/));
    chrome.test.assertTrue(originalWidth > 0);

    // Shrink the navigation list tree area to 50px.
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '50px'}]);

    // Check the navigation list area is scrolled completely to the left side.
    chrome.test.assertTrue(originalList.scrollLeft === 0);

    // Scroll the navigation list down (horizontal scroll).
    await remoteCall.callRemoteTestUtil(
        'setScrollLeft', appId, [navigationList, 100]);

    // Get the current state of the NavigationWidth width and left scroll
    // offset.
    const newList =
        await remoteCall.waitForElementStyles(appId, navigationList, ['width']);

    // Check that the width has been reduced.
    const newWidth = Number(newList.styles['width'].match(/[0-9]*/));
    chrome.test.assertTrue(newWidth < originalWidth);

    // Check: the navigation list should be scrolled.
    chrome.test.assertTrue(newList.scrollLeft > 0);
  };
})();
