// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Launches the file manager and opens a format dialog for the USB with label
 * |usbLabel|.
 */
async function openFormatDialog(usbLabel) {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
  await remoteCall.callRemoteTestUtil('overrideFormat', appId, []);

  // Focus the directory tree.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'focus', appId, ['#directory-tree']),
      'focus failed: #directory-tree');

  // Right click on the USB's directory tree entry.
  const treeQuery = `#directory-tree [entry-label="${usbLabel}"]`;
  await remoteCall.waitForElement(appId, treeQuery);
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, [treeQuery]),
      'fakeMouseRightClick failed');

  // Click on the format menu item.
  const formatItemQuery = '#roots-context-menu:not([hidden])' +
      ' cr-menu-item[command="#format"]:not([hidden]):not([disabled])';
  await remoteCall.waitAndClickElement(appId, formatItemQuery);

  // Check the dialog is open.
  await remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog[open]']);
  return appId;
}

/**
 * Tests the format dialog for a sample USB with files on it.
 */
testcase.formatDialog = async () => {
  await sendTestMessage({name: 'mountFakeUsb'});
  await sendTestMessage({name: 'mountUsbWithPartitions'});

  const appId = await openFormatDialog('fake-usb');

  // Check the correct size is displayed.
  const msg = await remoteCall.waitForElement(appId, [
    'files-format-dialog', '#warning-container:not([hidden]) #warning-message'
  ]);
  chrome.test.assertEq('51 bytes of files will be deleted', msg.text.trim());

  // Click format button.
  const formatButtonQuery = ['files-format-dialog', 'cr-button#format-button'];
  await remoteCall.waitAndClickElement(appId, formatButtonQuery);

  // Check the dialog is closed.
  remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog:not([open])']);
};

/**
 * Tests the format dialog for an empty USB.
 */
testcase.formatDialogEmpty = async () => {
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  const appId = await openFormatDialog('fake-usb');

  // Check the warning message is hidden.
  const warning = await remoteCall.waitForElement(
      appId, ['files-format-dialog', '#warning-container[fully-initialized]']);
  chrome.test.assertTrue(warning.hidden);

  // Click format button.
  const formatButtonQuery = ['files-format-dialog', 'cr-button#format-button'];
  await remoteCall.waitAndClickElement(appId, formatButtonQuery);

  // Check the dialog is closed.
  remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog:not([open])']);
};

/**
 * Tests cancelling out of the format dialog.
 */
testcase.formatDialogCancel = async () => {
  await sendTestMessage({name: 'mountFakeUsb'});

  const appId = await openFormatDialog('fake-usb');

  // Click cancel button.
  const formatButtonQuery = ['files-format-dialog', 'cr-button#cancel'];
  await remoteCall.waitAndClickElement(appId, formatButtonQuery);

  // Check the dialog is closed.
  remoteCall.waitForElement(
      appId, ['files-format-dialog', 'cr-dialog:not([open])']);
};
