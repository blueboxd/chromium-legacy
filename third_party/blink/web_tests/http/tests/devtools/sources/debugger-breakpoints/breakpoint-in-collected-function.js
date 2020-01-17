// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function test() {
  TestRunner.addResult(
      'Check that we can set breakpoint in collected function');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await SourcesTestRunner.startDebuggerTestPromise();
  TestRunner.addResult('evaluate script and collect function foo..');
  const script = `function foo() {
  return 1;
}
function boo() {
  return 2;
}
foo();
foo = null;
//# sourceURL=foo.js`;
  await TestRunner.evaluateInPageAnonymously(script);
  await Promise.all(SDK.targetManager.models(SDK.HeapProfilerModel)
                        .map(model => model.collectGarbage()));

  TestRunner.addResult('set breakpoint inside function foo and dump it..');
  let sourceFrame = await SourcesTestRunner.showScriptSourcePromise('foo.js');
  await SourcesTestRunner.createNewBreakpoint(sourceFrame, 1, '', true);
  await SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult(
      'run script again, dump pause location and inline breakpoints..');
  let inlineBreakpointsReady = SourcesTestRunner.waitDebuggerPluginBreakpoints();
  TestRunner.evaluateInPageAnonymously(script);
  let callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  SourcesTestRunner.captureStackTrace(callFrames);

  sourceFrame = await SourcesTestRunner.showScriptSourcePromise('foo.js');
  await inlineBreakpointsReady;
  await SourcesTestRunner.dumpDebuggerPluginBreakpoints(sourceFrame);

  SourcesTestRunner.completeDebuggerTest();
})();