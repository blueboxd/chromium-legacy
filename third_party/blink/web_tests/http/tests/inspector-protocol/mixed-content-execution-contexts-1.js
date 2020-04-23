(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that execution contexts are reported for frames that were blocked due to mixed content when runtime is enabled *before* navigation.`);
  await dp.Runtime.enable();
  let count = 0;
  dp.Runtime.onExecutionContextCreated(event => {
    testRunner.log(event);
    if (++count === 2) // page context + frame context.
      testRunner.completeTest();
  });
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/mixed-content-iframe.html');
})
