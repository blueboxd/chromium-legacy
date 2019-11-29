(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Target.setAutoAttach(windowOpen=true) attaches to window.open targets.`);

  await dp.Target.setDiscoverTargets({discover: true});

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true, windowOpen: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  session.evaluate(`
    window.myWindow = window.open('../resources/inspector-protocol-page.html'); undefined;
  `);
  testRunner.log('Opened the window');
  await attachedPromise;
  testRunner.log('Attached to window');

  const changedPromise = dp.Target.onceTargetInfoChanged();
  session.evaluate(`
    window.myWindow.location.assign('../resources/inspector-protocol-page.html?foo'); undefined;
  `);
  testRunner.log('Navigated the window');
  await changedPromise;
  testRunner.log('Target info changed');

  const detachedPromise = dp.Target.onceDetachedFromTarget();
  session.evaluate(`
    window.myWindow.close(); undefined;
  `);
  testRunner.log('Closed the window');
  await detachedPromise;
  testRunner.log('Detached from window');

  testRunner.completeTest();
})
