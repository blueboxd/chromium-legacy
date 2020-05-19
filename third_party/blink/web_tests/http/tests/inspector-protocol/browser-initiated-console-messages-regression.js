(async function(testRunner) {
  const {session, dp, page} = await testRunner.startBlank(
      `Check that multiple attached sessions don't crash the Log domain.`);
  const url = 'quic-transport://localhost';

  await dp.Log.enable();
  testRunner.log('Log in session 1 enabled');

  const dp2 = (await page.createSession()).protocol;
  await dp2.Log.enable();
  testRunner.log('Log in session 2 enabled');

  let onEntryAddedCallCount = 0;
  const onEntryAddedHandler = event => {
    testRunner.log(`Log.onEntryAdded: log message received`);

    onEntryAddedCallCount++;
    if (onEntryAddedCallCount == 2) {
      testRunner.completeTest();
    }
  }

  dp.Log.onEntryAdded(onEntryAddedHandler);
  dp2.Log.onEntryAdded(onEntryAddedHandler);

  // Taken from 'quic-transport-handshake-failure.js', failure to establish a connection
  // causes a error message to be logged to the DevTools console.
  testRunner.log('Trigger browser originating log message by instantiating QuicTransport.');
  session.evaluate(`new QuicTransport('${url}');`);
})
