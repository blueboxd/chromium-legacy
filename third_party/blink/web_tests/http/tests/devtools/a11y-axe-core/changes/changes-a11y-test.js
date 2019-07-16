// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests accessibility in the Changes drawer.');
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('changes');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('../../changes/resources/before.css');
  await TestRunner.addStylesheetTag('../../changes/resources/after.css');

  TestRunner.addResult('Loading files to compare.');
  const beforeSource = await TestRunner.waitForUISourceCode('before.css');
  const afterSource = await TestRunner.waitForUISourceCode('after.css');
  const content = await afterSource.requestContent();
  beforeSource.setWorkingCopy(content);

  TestRunner.addResult('Showing the Changes drawer.');
  await UI.viewManager.showView('changes.changes');
  const changesWidget = await UI.viewManager.view('changes.changes').widget();
  await TestRunner.addSnifferPromise(changesWidget, '_renderDiffRows');
  TestRunner.addResult(`Editor text content:\n${changesWidget._editor.text()}`);

  TestRunner.addResult('Running aXe on the Changes drawer.');
  await AxeCoreTestRunner.runValidation(changesWidget.contentElement);

  TestRunner.completeTest();
})();
