// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const crostiniTasks = {};

crostiniTasks.testShareBeforeOpeningDownloadsWithCrostiniApp = async (done) => {
  const fakeRoot = '#directory-tree [root-type-icon="crostini"]';

  // Save old fmp.getFileTasks and replace with version that returns
  // crostini app and chrome Text app.
  let oldGetFileTasks = chrome.fileManagerPrivate.getFileTasks;
  chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(callback, 0, [
      {
        taskId: 'text-app-id|app|text',
        title: 'Text',
        verb: 'open_with',
      },
      {
        taskId: 'crostini-app-id|crostini|open-with',
        title: 'Crostini App',
        verb: 'open_with',
        isDefault: true,
      }
    ]);
  };

  // Save old fmp.sharePathsWithCrostini.
  const oldSharePaths = chrome.fileManagerPrivate.sharePathsWithCrostini;
  let sharePathsCalled = false;
  let sharePathsPersist;
  chrome.fileManagerPrivate.sharePathsWithCrostini =
      (vmName, entry, persist, callback) => {
        sharePathsCalled = true;
        sharePathsPersist = persist;
        oldSharePaths(vmName, entry, persist, callback);
      };

  // Save old fmp.executeTask.
  const oldExecuteTask = chrome.fileManagerPrivate.executeTask;
  let executeTaskCalled = false;
  chrome.fileManagerPrivate.executeTask = (taskId, entries, callback) => {
    executeTaskCalled = true;
    oldExecuteTask(taskId, entries, callback);
  };
  chrome.metricsPrivate.values_ = [];

  await test.setupAndWaitUntilReady([], [], []);

  // Add '/A', and '/A/hello.txt', refresh, 'A' is shown.
  test.addEntries([test.ENTRIES.directoryA, test.ENTRIES.helloInA], [], []);
  test.refreshFileList();
  await test.waitForFiles(test.TestEntryInfo.getExpectedRows(
      [test.ENTRIES.directoryA, test.ENTRIES.linuxFiles]));

  // Change to 'A' directory, hello.txt is shown.
  assertTrue(test.fakeMouseDoubleClick('[file-name="A"]'));
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows([test.ENTRIES.hello]));

  // Right click on 'hello.txt' file, wait for dialog with 'Open with'.
  assertTrue(test.fakeMouseRightClick('[file-name="hello.txt"]'));
  await test.waitForElement('cr-menu-item[command="#open-with"]:not([hidden]');

  // Click 'Open with', wait for picker.
  assertTrue(test.fakeMouseClick('cr-menu-item[command="#open-with"]'));
  await test.waitForElement('#default-tasks-list');

  // Ensure that the default tasks label is shown correctly.
  const item = document.querySelector('#default-task-menu-item span');
  assertEquals('Open with Crostini App', item.innerText);

  // Ensure picker shows both options.  Click on 'Crostini App'.  Ensure
  // share path dialog is shown.
  const list = document.querySelectorAll('#default-tasks-list li div');
  assertEquals(2, list.length);
  assertEquals('Crostini App (default)', list[0].innerText);
  assertEquals('Open with Text', list[1].innerText);
  assertTrue(test.fakeMouseClick('#default-tasks-list li'));
  // Ensure fmp.sharePathsWithCrostini, fmp.executeTask called.
  await test.repeatUntil(() => {
    return sharePathsCalled && executeTaskCalled ||
        test.pending('Waiting to share and open');
  });

  // Share should not persist as a result of open with crostini app.
  assertFalse(sharePathsPersist);
  // Validate UMAs.
  const lastEnumUma = chrome.metricsPrivate.values_.pop();
  assertEquals('FileBrowser.CrostiniShareDialog', lastEnumUma[0].metricName);
  assertEquals(1 /* ShareBeforeOpen */, lastEnumUma[1]);

  // Restore fmp.*.
  chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
  chrome.fileManagerPrivate.sharePathsWithCrostini = oldSharePaths;
  chrome.fileManagerPrivate.executeTask = oldExecuteTask;
  chrome.fileManagerPrivate.removeMount('crostini');
  await test.waitForElement(fakeRoot);
  done();
};

crostiniTasks.testErrorLoadingLinuxPackageInfo = async (done) => {
  const fakeRoot = '#directory-tree [root-type-icon="crostini"]';
  const dialog = '#install-linux-package-dialog';
  const detailsFrame = '.install-linux-package-details-frame';

  // Save old fmp.getFileTasks and replace with version that returns
  // the internal linux package install handler
  let oldGetFileTasks = chrome.fileManagerPrivate.getFileTasks;
  chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback, 0, [{
          taskId: `${test.FILE_MANAGER_EXTENSION_ID}|app|install-linux-package`,
          title: 'Install with Linux (Beta)',
          verb: 'open_with',
          isDefault: true,
        }]);
  };

  // Save old fmp.getLinuxPackageInfo and replace with version that saves the
  // callback to manually call later
  let oldGetLinuxPackageInfo = chrome.fileManagerPrivate.getLinuxPackageInfo;
  let packageInfoCallback = null;
  chrome.fileManagerPrivate.getLinuxPackageInfo = (entry, callback) => {
    packageInfoCallback = callback;
  };

  await test.setupAndWaitUntilReady([], [], [test.ENTRIES.debPackage]);
  await test.waitForElement(fakeRoot);

  // Select 'Linux files' in directory tree.
  assertTrue(test.fakeMouseClick(fakeRoot), 'click Linux files');
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows([test.ENTRIES.debPackage]));

  // Double click on 'package.deb' file to open the install dialog.
  assertTrue(test.fakeMouseDoubleClick('[file-name="package.deb"]'));
  await test.waitForElement(dialog);

  // Verify the loading state is shown.
  assertEquals(
      'Details\nLoading information...',
      document.querySelector(detailsFrame).innerText);
  await test.repeatUntil(() => {
    return packageInfoCallback ||
        test.pending('Waiting for package info request');
  });

  // Call the callback with an error.
  chrome.runtime.lastError = {message: 'error message'};
  packageInfoCallback(undefined);
  delete chrome.runtime.lastError;
  assertEquals(
      'Details\nFailed to retrieve app info.',
      document.querySelector(detailsFrame).innerText);

  // Click 'cancel' to close.  Ensure dialog closes.
  assertTrue(test.fakeMouseClick('button.cr-dialog-cancel'));
  await test.waitForElementLost(dialog);

  // Restore fmp.getFileTasks, fmp.getLinuxPackageInfo.
  chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
  chrome.fileManagerPrivate.getLinuxPackageInfo = oldGetLinuxPackageInfo;
  chrome.fileManagerPrivate.removeMount('crostini');
  await test.waitForElement(fakeRoot);
  done();
};

crostiniTasks.testImportCrostiniImage = async (done) => {
  const dialog = '.cr-dialog-container.shown';

  // Save old fmp.importCrostiniImage function.
  const oldImportCrostiniImage = chrome.fileManagerPrivate.importCrostiniImage;
  let importCrostiniImageCalled = false;
  chrome.fileManagerPrivate.importCrostiniImage = (entry) => {
    importCrostiniImageCalled = true;
  };

  // Save old fmp.getFileTasks and replace with version that returns the
  // internal import crostini image handler.
  const oldGetFileTasks = chrome.fileManagerPrivate.getFileTasks;
  chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback, 0, [{
          taskId: `${test.FILE_MANAGER_EXTENSION_ID}|app|import-crostini-image`,
        }]);
  };

  await test.setupAndWaitUntilReady([test.ENTRIES.tiniFile]);

  assertTrue(test.fakeMouseDoubleClick('[file-name="test.tini"]'));
  await test.waitForElement(dialog);

  assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
  await test.waitForElementLost(dialog);

  assertTrue(importCrostiniImageCalled);

  // Restore fmp.getFileTasks, fmp.importCrostiniImage.
  chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
  chrome.fileManagerPrivate.importCrostiniImage = oldImportCrostiniImage;
  done();
};
