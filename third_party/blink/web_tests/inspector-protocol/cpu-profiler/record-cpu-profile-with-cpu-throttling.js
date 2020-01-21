(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Test that the profiler can record a profile with cpu throttling enabled.');

  await dp.Emulation.setCPUThrottlingRate({rate: 4});
  await dp.Profiler.enable();
  await dp.Profiler.start();

  await session.evaluate(`
    let count = 0;
    for (let i = 0; i < 1e7; i++) {
      count += i;
    }
    window.count = count;
  `);

  await dp.Profiler.stop();

  testRunner.completeTest();
})
