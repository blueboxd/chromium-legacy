(async function() {
  TestRunner.addResult(
      `Tests that reloading while paused at a breakpoint doesn't execute code after the breakpoint.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await SourcesTestRunner.startDebuggerTestPromise(true);
  TestRunner.addResult('Setting break on all exceptions.');
  TestRunner.DebuggerAgent.setPauseOnExceptions(
      SDK.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions);
  SourcesTestRunner.waitUntilPaused(step1);
  await TestRunner.navigatePromise(
      '../../sources/debugger-breakpoints/resources/diverge-without-breakpoint-throw-on-load.html');
  TestRunner.evaluateInPageWithTimeout(`divergingFunctionWithThrow()`);
  async function step1(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    TestRunner.addResult('Reloading page...');
    TestRunner.reloadPage(onPageReloaded);
  }
  function onPageReloaded() {
    TestRunner.completeTest();
  }
})();
