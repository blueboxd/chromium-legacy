(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test post data interception`);

  await dp.Network.enable();
  await dp.Fetch.enable();

  const [requestWillBeSent, requestPaused] = await Promise.all([
    dp.Network.onceRequestWillBeSent(),
    dp.Fetch.onceRequestPaused(),
    session.evaluate(`fetch('${testRunner.url('./resources/hello-world.txt')}', {
                          method: 'post',
                          body: new Uint8Array([1, 2, 3, 4, 5])
                      })`)
  ]);
  testRunner.log(requestWillBeSent.params.request.postDataEntries);
  testRunner.log(requestPaused.params.request.postDataEntries);
  testRunner.completeTest();
})
