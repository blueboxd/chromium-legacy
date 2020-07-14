(async function (testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Tests that the ad frame type is reported on navigation\n`);
  await dp.Page.enable();
  session.evaluate(`
    if (window.testRunner) {
      // Inject a subresource filter to mark 'ad-iframe-writer.js' as a would be disallowed resource.
      testRunner.setDisallowedSubresourcePathSuffixes(["ad-iframe-writer.js"], false /* block_subresources */);
      testRunner.setHighlightAds();
    }

    // Script must be loaded after disallowed paths are set to be marked as an ad.
    let ad_script = document.createElement("script");
    ad_script.async = false;
    ad_script.src = "../resources/ad-iframe-writer.js";
    ad_script.onload = function () {
      ad_frame = createAdFrame();
      ad_frame.width = 100;
      ad_frame.height = 200;
    };
    document.body.appendChild(ad_script);
  `);
  const { params } = await dp.Page.onceFrameNavigated();
  testRunner.log({ adFrameType: params.frame.adFrameType });

  testRunner.completeTest();
})
