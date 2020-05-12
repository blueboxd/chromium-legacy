(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Check the console message printed on a QuicTransport handshake failure.`);
  const url = 'quic-transport://localhost';

  await dp.Log.enable();
  testRunner.log('Log Enabled');

  dp.Log.onEntryAdded(event => {
    const entry = event.params.entry;
    testRunner.log('Log.onEntryAdded');
    testRunner.log(`source: ${entry.source}`);
    testRunner.log(`level: ${entry.level}`);
    testRunner.log(`text: ${entry.text}`);
    testRunner.completeTest();
  });

  session.evaluate(`new QuicTransport('${url}');`);
  testRunner.log('Instantiate QuicTransport.');
})
