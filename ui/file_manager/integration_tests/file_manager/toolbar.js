// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests that the Delete menu item is disabled if no entry is selected.
 */
testcase.toolbarDeleteWithMenuItemNoEntrySelected = async () => {
  const contextMenu = '#file-context-menu:not([hidden])';

  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Right click the list without selecting an entry.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['list.list']),
      'fakeMouseRightClick failed');

  // Wait until the context menu is shown.
  await remoteCall.waitForElement(appId, contextMenu);

  // Assert the menu delete command is disabled.
  const deleteDisabled = '[command="#delete"][disabled="disabled"]';
  await remoteCall.waitForElement(appId, contextMenu + ' ' + deleteDisabled);
};

/**
 * Tests Delete button keeps focus after closing confirmation
 * dialog.
 */
testcase.toolbarDeleteButtonKeepFocus = async () => {
  const entries = [ENTRIES.desktop];

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);

  // Select My Desktop Background.png
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [ENTRIES.desktop.nameText]));

  await remoteCall.simulateUiClick(appId, '#delete-button');

  // Confirm that the confirmation dialog is shown.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Press cancel button.
  await remoteCall.waitAndClickElement(appId, 'button.cr-dialog-cancel');

  // Check focused element is Delete button.
  const focusedElement =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  chrome.test.assertEq('delete-button', focusedElement.attributes['id']);
};

/**
 * Tests deleting an entry using the toolbar.
 */
testcase.toolbarDeleteEntry = async () => {
  const beforeDeletion = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.beautiful,
  ]);

  const afterDeletion = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.beautiful,
  ]);

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Confirm entries in the directory before the deletion.
  await remoteCall.waitForFiles(
      appId, beforeDeletion, {ignoreLastModifiedTime: true});

  // Select My Desktop Background.png
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, ['My Desktop Background.png']));


  // Click delete button in the toolbar.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#delete-button']));


  // Confirm that the confirmation dialog is shown.
  await remoteCall.waitForElement(appId, '.cr-dialog-container.shown');

  // Press delete button.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['button.cr-dialog-ok']),
      'fakeMouseClick failed');

  // Confirm the file is removed.
  await remoteCall.waitForFiles(
      appId, afterDeletion, {ignoreLastModifiedTime: true});
};

/**
 * Tests that refresh button hides in selection mode.
 * Non-watchable volumes display refresh button so users can refresh the file
 * list content. However this button should be hidden when entering the
 * selection mode. crbug.com/978383
 *
 */
testcase.toolbarRefreshButtonWithSelection = async () => {
  // Enable media views which are non-watchable.
  await sendTestMessage({name: 'mountMediaView'});

  // Add some content to media view "Images".
  await addEntries(['media_view_images'], [ENTRIES.desktop]);

  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Navigate to Images media view.
  await remoteCall.waitAndClickElement(
      appId, '#directory-tree [entry-label="Images"]');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Images');

  // Check that refresh button is visible.
  await remoteCall.waitForElement(appId, '#refresh-button:not([hidden])');

  // Ctrl+A to enter selection mode.
  const ctrlA = ['#file-list', 'a', true, false, false];
  await remoteCall.fakeKeyDown(appId, ...ctrlA);

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that refresh button is not shown when Recent is selected.
 */
testcase.toolbarRefreshButtonHiddenInRecents = async () => {
  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Navigate to Recent.
  await remoteCall.waitAndClickElement(
      appId, '#directory-tree [entry-label="Recent"]');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/Recent');

  // Check that the button should be hidden.
  await remoteCall.waitForElement(appId, '#refresh-button[hidden]');
};

/**
 * Tests that command Alt+A focus the toolbar.
 */
testcase.toolbarAltACommand = async () => {
  // Open files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // Press Alt+A in the File List.
  const altA = ['#file-list', 'a', false, false, true];
  await remoteCall.fakeKeyDown(appId, ...altA);

  // Check that a menu-button should be focused.
  const focusedElement =
      await remoteCall.callRemoteTestUtil('getActiveElement', appId, []);
  const cssClasses = focusedElement.attributes['class'] || '';
  chrome.test.assertTrue(cssClasses.includes('menu-button'));
};

/**
 * Tests that the menu drop down follows the button if the button moves. This
 * happens when the search box is expanded and then collapsed.
 */
testcase.toolbarMultiMenuFollowsButton = async () => {
  const entry = ENTRIES.hello;

  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Override the tasks so the "Open" button becomes a dropdown button.
  await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [DOWNLOADS_FAKE_TASKS]);

  // Select an entry in the file list.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  // Click the toolbar search button.
  await remoteCall.waitAndClickElement(appId, '#search-button');

  // Wait for the search box to expand.
  await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

  // Click the toolbar "Open" dropdown button.
  await remoteCall.simulateUiClick(appId, '#tasks');

  // Wait for the search box to collapse.
  await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  // Check that the dropdown menu and "Open" button are aligned.
  const caller = getCaller();
  await repeatUntil(async () => {
    const openButton =
        await remoteCall.waitForElementStyles(appId, '#tasks', ['width']);
    const menu =
        await remoteCall.waitForElementStyles(appId, '#tasks-menu', ['width']);

    if (openButton.renderedLeft !== menu.renderedLeft) {
      return pending(
          caller,
          `Waiting for the menu and button to be aligned: ` +
              `${openButton.renderedLeft} != ${menu.renderedLeft}`);
    }
  });
};
