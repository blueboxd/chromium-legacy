(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank('Tests that device emulation of window segments is propagated and powers getWindowSegments API.');

  let deviceMetrics = {
    width: 800,
    height: 600,
    deviceScaleFactor: 2,
    mobile: false,
    fitWindow: false,
    scale: 2,
    screenWidth: 1200,
    screenHeight: 1000,
    positionX: 110,
    positionY: 120,
  };

  await session.protocol.Emulation.setDeviceMetricsOverride(deviceMetrics);

  await session.navigate('../resources/device-emulation.html');

  testRunner.log("No segments:");
  testRunner.log(await session.evaluate(`dumpWindowSegments()`));

  testRunner.log("Side-by-side segments");
  deviceMetrics.displayFeature = {
      orientation: "vertical",
      offset: 390,
      maskLength: 20
  };
  await session.protocol.Emulation.setDeviceMetricsOverride(deviceMetrics);
  testRunner.log(await session.evaluate(`dumpWindowSegments()`));

  testRunner.log("Unspecified display feature");
  delete deviceMetrics.displayFeature;
  await session.protocol.Emulation.setDeviceMetricsOverride(deviceMetrics);
  testRunner.log(await session.evaluate(`dumpWindowSegments()`));

  testRunner.log("Stacked segments");
  deviceMetrics.displayFeature = {
      orientation: "horizontal",
      offset: 290,
      maskLength: 20
  };
  await session.protocol.Emulation.setDeviceMetricsOverride(deviceMetrics);
  testRunner.log(await session.evaluate(`dumpWindowSegments()`));

  testRunner.log("Emulation disabled");
  await dp.Emulation.clearDeviceMetricsOverride();
  testRunner.log(await session.evaluate(`dumpWindowSegments()`));

  testRunner.completeTest();
})
