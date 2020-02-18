(async function() {
  TestRunner.addResult(
      `Tests that reloading while paused at a breakpoint doesn't execute code after the breakpoint.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await SourcesTestRunner.startDebuggerTestPromise(true);
  await TestRunner.navigatePromise(
      '../../sources/debugger-breakpoints/resources/diverge-without-breakpoint.html');

  const sourceFrame = await SourcesTestRunner.showScriptSourcePromise(
      'diverge-without-breakpoint.html');
  TestRunner.addResult('Setting a breakpoint.');
  await SourcesTestRunner.createNewBreakpoint(sourceFrame, 4, '', true)
  SourcesTestRunner.waitUntilPaused(step1);

  TestRunner.evaluateInPageWithTimeout(`divergingFunction()`);

  async function step1(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    TestRunner.addResult('Reloading page...');
    TestRunner.reloadPage(onPageReloaded);
  }

  function onPageReloaded() {
    TestRunner.completeTest();
  }
})();
