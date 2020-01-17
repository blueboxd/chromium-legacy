// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Tests that when the current folder is changed, the 'active' attribute
   * appears in the active folder .tree-row.
   */
  testcase.directoryTreeActiveDirectory = async () => {
    const activeRow = '.tree-row[selected][active]';

    // Open FilesApp on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

    // Change to My files folder.
    await navigateWithDirectoryTree(appId, '/My files');

    // Check: the My files folder should be the active tree row.
    const myFiles = await remoteCall.waitForElement(appId, activeRow);
    chrome.test.assertTrue(myFiles.text.includes('My files'));

    // Change to Downloads folder.
    await navigateWithDirectoryTree(appId, '/My files/Downloads');

    // Check: the Downloads folder should be the active tree row.
    const downloads = await remoteCall.waitForElement(appId, activeRow);
    chrome.test.assertTrue(downloads.text.includes('Downloads'));

    // Change to Google Drive volume's My Drive folder.
    await navigateWithDirectoryTree(appId, '/My Drive');

    // Check: the My Drive folder should be the active tree row.
    const myDrive = await remoteCall.waitForElement(appId, activeRow);
    chrome.test.assertTrue(myDrive.text.includes('My Drive'));

    // Change to Recent folder.
    await navigateWithDirectoryTree(appId, '/Recent');

    // Check: the Recent folder should be the active tree row.
    const recent = await remoteCall.waitForElement(appId, activeRow);
    chrome.test.assertTrue(recent.text.includes('Recent'));
  };

  /**
   * Tests that the active folder does not change when the directory tree
   * selected folder is changed, but does change when the selected folder
   * is activated (via the Enter key for example).
   */
  testcase.directoryTreeSelectedDirectory = async () => {
    // Open FilesApp on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

    // Change to My files folder.
    await navigateWithDirectoryTree(appId, '/My files');

    // Check: the My files folder should be [selected] and [active].
    const selectedActiveRow = '.tree-row[selected][active]';
    let treeRow = await remoteCall.waitForElement(appId, selectedActiveRow);
    chrome.test.assertTrue(treeRow.text.includes('My files'));

    // Send ArrowUp key to change the selected folder.
    const arrowUp = ['#directory-tree', 'ArrowUp', false, false, false];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, arrowUp);

    // Check: no folder should be [selected] and [active].
    await remoteCall.waitForElementLost(appId, selectedActiveRow);

    // Check: the My files folder should be [active].
    treeRow = await remoteCall.waitForElement(appId, '.tree-row[active]');
    chrome.test.assertTrue(treeRow.text.includes('My files'));

    // Check: the Recent folder should be [selected].
    treeRow = await remoteCall.waitForElement(appId, '.tree-row[selected]');
    chrome.test.assertTrue(treeRow.text.includes('Recent'));

    // Send Enter key to activate the selected folder.
    const enter = ['#directory-tree', 'Enter', false, false, false];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, enter);

    // Check: the Recent folder should be [selected] and [active].
    treeRow = await remoteCall.waitForElement(appId, selectedActiveRow);
    chrome.test.assertTrue(treeRow.text.includes('Recent'));
  };

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
   * Tests that the directory tree does not horizontally scroll when expanding
   * nested folder items.
   */
  testcase.directoryTreeExpandHorizontalScroll = async () => {
    /**
     * Creates a folder test entry from a folder |path|.
     * @param {string} path The folder path.
     * @return {!TestEntryInfo}
     */
    function createFolderTestEntry(path) {
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
    for (let path = 'nested-folder1', i = 0; i < 6; ++i) {
      nestedFolderTestEntries.push(createFolderTestEntry(path));
      path += `/nested-folder${i + 1}`;
    }

    // Open FilesApp on Downloads containing the folder test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Verify the directory tree is not horizontally scrolled.
    const directoryTree = '#directory-tree';
    const original = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    chrome.test.assertTrue(original.scrollLeft === 0);

    // Shrink the tree to 150px, enough to hide the deep folder names.
    const navigationList = '.dialog-navigation-list';
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '150px'}]);

    // Expand the tree Downloads > nested-folder1 > nested-folder2 ...
    const lastFolderPath = nestedFolderTestEntries.pop().targetPath;
    await remoteCall.navigateWithDirectoryTree(
        appId, '/Downloads/' + lastFolderPath, 'My files');

    // Check: the directory tree should be showing the last test entry.
    await remoteCall.waitForElement(
        appId, '.tree-item[entry-label="nested-folder5"]:not([hidden])');

    // Ensure the directory tree scroll event handling is complete.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'requestAnimationFrame', appId, []));

    // Check: the directory tree should not be horizontally scrolled.
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    const notScrolled = scrolled.scrollLeft === 0;
    chrome.test.assertTrue(notScrolled, 'Tree should not scroll left');
  };

  /**
   * Tests that the directory tree does not horizontally scroll when expanding
   * nested folder items when the text direction is RTL.
   */
  testcase.directoryTreeExpandHorizontalScrollRTL = async () => {
    /**
     * Creates a folder test entry from a folder |path|.
     * @param {string} path The folder path.
     * @return {!TestEntryInfo}
     */
    function createFolderTestEntry(path) {
      const name = path.split('/').pop();
      return new TestEntryInfo({
        targetPath: path,
        nameText: name,
        type: EntryType.DIRECTORY,
        lastModifiedTime: 'Dec 28, 1962, 10:42 PM',
        sizeText: '--',
        typeText: 'Folder',
      });
    }

    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = [];
    for (let path = 'nested-folder1', i = 0; i < 6; ++i) {
      nestedFolderTestEntries.push(createFolderTestEntry(path));
      path += `/nested-folder${i + 1}`;
    }

    // Open FilesApp on Downloads containing the folder test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Redraw FilesApp with text direction RTL.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'renderWindowTextDirectionRTL', appId, []));

    // Verify the directory tree is not horizontally scrolled.
    const directoryTree = '#directory-tree';
    const original = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    chrome.test.assertTrue(original.scrollLeft === 0);

    // Shrink the tree to 150px, enough to hide the deep folder names.
    const navigationList = '.dialog-navigation-list';
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '150px'}]);

    // Expand the tree Downloads > nested-folder1 > nested-folder2 ...
    const lastFolderPath = nestedFolderTestEntries.pop().targetPath;
    await remoteCall.navigateWithDirectoryTree(
        appId, '/Downloads/' + lastFolderPath, 'My files');

    // Check: the directory tree should be showing the last test entry.
    await remoteCall.waitForElement(
        appId, '.tree-item[entry-label="nested-folder5"]:not([hidden])');

    // Ensure the directory tree scroll event handling is complete.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'requestAnimationFrame', appId, []));

    // Get the scroll limits: see crbug.com/721759 for RTL |scrollRight|.
    const scrolled = await remoteCall.waitForElementStyles(
        appId, directoryTree, ['scrollLeft']);
    const scrollRight = scrolled.scrollWidth - scrolled.renderedWidth;

    // Check: the directory tree should not be horizontally scrolled.
    const notScrolled = scrolled.scrollLeft === scrollRight;
    chrome.test.assertTrue(notScrolled, 'Tree should not scroll right');
  };

  /**
   * Tests that the directory tree element has a clipped attribute when the
   * tree is narrowed in width due to a window.resize event.
   */
  testcase.directoryTreeClippedWindowResize = async () => {
    // Open FilesApp on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

    // Verify the directory tree is not initially clipped.
    await remoteCall.waitForElement(appId, '#directory-tree:not([clipped])');

    // Change the directory tree to a narrow width and fire window.resize.
    const navigationList = '.dialog-navigation-list';
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '100px'}]);
    await remoteCall.callRemoteTestUtil('fakeResizeEvent', appId, []);

    // Check: the directory tree should report that it is clipped.
    await remoteCall.waitForElement(appId, '#directory-tree[clipped]');

    // Change the directory tree to a wider width and fire window.resize.
    await remoteCall.callRemoteTestUtil(
        'setElementStyles', appId, [navigationList, {width: '300px'}]);
    await remoteCall.callRemoteTestUtil('fakeResizeEvent', appId, []);

    // Check: the directory tree should report that it is not clipped.
    await remoteCall.waitForElementLost(appId, '#directory-tree[clipped]');
  };

  /**
   * Tests that the directory tree element has a clipped attribute when the
   * tree is narrowed in width due to the splitter.
   */
  testcase.directoryTreeClippedSplitterResize = async () => {
    const splitter = '#navigation-list-splitter';

    /**
     * Creates a left button mouse event |name| targeted at the splitter,
     * located at position (x,y) relative to the splitter bounds.
     * @param {string} name The mouse event name.
     * @param {number} x The event.clientX location.
     * @param {number} y The event.clientY location.
     * @return {!Array<*>} remoteCall fakeEvent arguments array.
     */
    function createSplitterMouseEvent(name, x, y) {
      const fakeEventArguments = [splitter, name];

      fakeEventArguments.push({
        // Event properties of the fakeEvent.
        'bubbles': true,
        'composed': true,
        'button': 0,  // Main button: left usually.
        'clientX': x,
        'clientY': y,
      });

      return fakeEventArguments;
    }

    // Open FilesApp on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

    // Verify the directory tree is not initially clipped.
    await remoteCall.waitForElement(appId, '#directory-tree:not([clipped])');

    // Send a mouse down to the splitter element.
    await remoteCall.waitForElement(appId, splitter);
    const mouseDown = createSplitterMouseEvent('mousedown', 0, 0);
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeEvent', appId, mouseDown));

    // Mouse is down: move it to the left.
    const moveLeft = createSplitterMouseEvent('mousemove', -200, 0);
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeEvent', appId, moveLeft));

    // Check: the directory tree should report that it is clipped.
    await remoteCall.waitForElement(appId, '#directory-tree[clipped]');

    // Mouse is down: move it to the right.
    const moveRight = createSplitterMouseEvent('mousemove', 200, 0);
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeEvent', appId, moveRight));

    // Check: the directory tree should report that it is not clipped.
    await remoteCall.waitForElementLost(appId, '#directory-tree[clipped]');
  };
})();
