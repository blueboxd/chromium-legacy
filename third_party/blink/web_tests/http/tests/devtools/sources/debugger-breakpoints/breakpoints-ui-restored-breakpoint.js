// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests breakpoint is restored.');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addScriptTag('resources/a.js');

  // Pairs of line number plus breakpoint decoration counts.
  // We expect line 5, 9 and 10 to have decoration each.
  const expectedDecorations = [[5, 1], [9, 1], [10, 1]];

  let originalSourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');
  TestRunner.addResult('Set different breakpoints and dump them');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(originalSourceFrame, expectedDecorations, async () => {
    await SourcesTestRunner.toggleBreakpoint(originalSourceFrame, 9, false);
    await SourcesTestRunner.createNewBreakpoint(originalSourceFrame, 10, 'a === 3', true);
    await SourcesTestRunner.createNewBreakpoint(originalSourceFrame, 5, '', false);
  });

  TestRunner.addResult('Reload page and add script again and dump breakpoints');
  await TestRunner.reloadPagePromise();
  await TestRunner.addScriptTag(TestRunner.url('resources/a.js'));
  let sourceFrameAfterReload = await SourcesTestRunner.showScriptSourcePromise('a.js');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
      sourceFrameAfterReload, expectedDecorations, () => {}, true);

  // TODO(kozyatinskiy): as soon as we have script with the same url in different frames
  // everything looks compeltely broken, we should fix it.
  TestRunner.addResult('Added two more iframes with script with the same url');
  TestRunner.addIframe(TestRunner.url('resources/frame-with-script.html'));
  TestRunner.addIframe(TestRunner.url('resources/frame-with-script.html'));
  const uiSourceCodes = await waitForNScriptSources('a.js', 3);

  // The disabled breakpoint at line 5 is currently not included in the iframes that are added.
  const expectedDecorationsArray = [[[9, 1], [10, 1]], [[9, 1], [10, 1]], expectedDecorations];
  let index = 0;
  for (const uiSourceCode of uiSourceCodes) {
    TestRunner.addResult('Show uiSourceCode and dump breakpoints');
    const sourceFrame = await SourcesTestRunner.showUISourceCodePromise(uiSourceCode);
    await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
        sourceFrame, expectedDecorationsArray[index++], () => {}, true);
  }

  TestRunner.addResult('Reload page and add script again and dump breakpoints');
  await TestRunner.reloadPagePromise();
  await TestRunner.addScriptTag(TestRunner.url('resources/a.js'));
  sourceFrameAfterReload = await SourcesTestRunner.showScriptSourcePromise('a.js');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
      sourceFrameAfterReload, expectedDecorations, () => {}, true);

  TestRunner.completeTest();

  async function waitForNScriptSources(scriptName, N) {
    while (true) {
      let uiSourceCodes = UI.panels.sources._workspace.uiSourceCodes();
      uiSourceCodes = uiSourceCodes.filter(uiSourceCode => uiSourceCode.project().type() !== Workspace.projectTypes.Service && uiSourceCode.name() === scriptName);
      if (uiSourceCodes.length === N)
        return uiSourceCodes;
      await TestRunner.addSnifferPromise(Sources.SourcesView.prototype, '_addUISourceCode');
    }
  }
})();
