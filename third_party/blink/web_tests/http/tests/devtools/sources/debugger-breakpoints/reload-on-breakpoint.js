(async function() {
  TestRunner.addResult(
      `Tests that reloading while paused at a breakpoint doesn't execute code after the breakpoint.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.evaluateInPagePromise(`
      function divergingFunction() {
          debugger;
          while(true) {};
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([function testFetchBreakpoint(next) {
    SourcesTestRunner.waitUntilPaused(step1);
    TestRunner.addResult('Waiting for breakpoint.');
    TestRunner.evaluateInPageWithTimeout('divergingFunction()');

    async function step1(callFrames) {
      await SourcesTestRunner.captureStackTrace(callFrames);
      TestRunner.addResult('Reloading page...');
      TestRunner.reloadPage(onPageReloaded);
    }

    function onPageReloaded() {
      next();
    }
  }]);
})();
