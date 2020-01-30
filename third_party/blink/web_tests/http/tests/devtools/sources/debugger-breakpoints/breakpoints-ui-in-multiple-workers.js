// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests breakpoint works in multiple workers.');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  // Pairs of line number plus breakpoint decoration counts.
  // We expected line 1-3 to have one decoration each.
  const expectedDecorations = [[1, 1], [2, 1], [3, 1]];

  await addWorker('resources/worker.js');
  let workerSourceFrame = await SourcesTestRunner.showScriptSourcePromise('worker.js');
  TestRunner.addResult('Set different breakpoints and dump them');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
      workerSourceFrame, expectedDecorations, async () => {
    await SourcesTestRunner.toggleBreakpoint(workerSourceFrame, 1, false);
    await SourcesTestRunner.createNewBreakpoint(workerSourceFrame, 2, 'a === 3', true);
    await SourcesTestRunner.createNewBreakpoint(workerSourceFrame, 3, '', false);
  });

  TestRunner.addResult('Reload page and add script again and dump breakpoints');
  await TestRunner.reloadPagePromise();
  await addWorker(TestRunner.url('resources/worker.js'));
  let sourceFrameAfterReload = await SourcesTestRunner.showScriptSourcePromise('worker.js');
  await SourcesTestRunner.waitForExactBreakpointDecorations(
      sourceFrameAfterReload, expectedDecorations, true);
  SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrameAfterReload);

  TestRunner.addResult('Added two more workers');
  await addWorker(TestRunner.url('resources/worker.js'));
  await addWorker(TestRunner.url('resources/worker.js'));
  const uiSourceCodes = await waitForNScriptSources('worker.js', 3);

  // The disabled breakpoint on line 3 is not included in the newly added workers.
  const expectedDecorationsArray = [
      [[1, 1], [2, 1]],
      [[1, 1], [2, 1]],
      expectedDecorations
  ];
  let index = 0;
  for (const uiSourceCode of uiSourceCodes) {
    TestRunner.addResult('Show uiSourceCode and dump breakpoints');
    const sourceFrame = await SourcesTestRunner.showUISourceCodePromise(uiSourceCode);
    await SourcesTestRunner.waitForExactBreakpointDecorations(
        sourceFrame, expectedDecorationsArray[index++], true);
    SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);
  }

  TestRunner.addResult('Test breakpoint in each worker');
  await SourcesTestRunner.startDebuggerTestPromise();
  for (var i = 0; i < 3; ++i) {
    const pausedPromise = SourcesTestRunner.waitUntilPausedPromise();

    await TestRunner.evaluateInPageAsync(`window.workers[${i}].postMessage('')`);

    const callFrames = await pausedPromise;
    SourcesTestRunner.captureStackTrace(callFrames);
    await new Promise(resolve => SourcesTestRunner.resumeExecution(resolve));
  }
  SourcesTestRunner.completeDebuggerTest();

  async function waitForNScriptSources(scriptName, N) {
    while (true) {
      let uiSourceCodes = UI.panels.sources._workspace.uiSourceCodes();
      uiSourceCodes = uiSourceCodes.filter(uiSourceCode => uiSourceCode.project().type() !== Workspace.projectTypes.Service && uiSourceCode.name() === scriptName);
      if (uiSourceCodes.length === N)
        return uiSourceCodes;
      await TestRunner.addSnifferPromise(Sources.SourcesView.prototype, '_addUISourceCode');
    }
  }

  function addWorker(url) {
    return TestRunner.evaluateInPageAsync(`
      (function(){
        window.workers = window.workers || [];
        window.workers.push(new Worker('${url}'));
      })();
    `);
  }
})();