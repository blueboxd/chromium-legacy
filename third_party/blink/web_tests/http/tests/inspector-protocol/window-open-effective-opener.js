(async function(testRunner) {
    var {page, session, dp} = await testRunner.startBlank('Tests for correct opener in Page.TargetCreated and Page.TargetInfoChanged protocol method.');
    await dp.Target.setDiscoverTargets({discover: true});
    await dp.Page.enable();

    testRunner.log(`\nOpening without new browsing context`);
    session.evaluate(`window.open('./resources/test-page.html', '_blank'); undefined`);
    var response = await dp.Target.onceTargetCreated();
    testRunner.log(response.params.targetInfo);
    response = await dp.Target.onceTargetInfoChanged();
    testRunner.log(response.params.targetInfo);

    testRunner.log(`\nOpening with new browsing context`);
    session.evaluate(`window.open('./resources/test-page.html', '_blank', 'noopener'); undefined`);
    response = await dp.Target.onceTargetCreated();
    testRunner.log(response.params.targetInfo);
    response = await dp.Target.onceTargetInfoChanged();
    testRunner.log(response.params.targetInfo);

    testRunner.log(`\nOpening with COOP header`);
    await dp.Page.navigate({ url: testRunner.url('https://127.0.0.1:8443/inspector-protocol/resources/coop.php')});
    session.evaluate(`window.open('./resources/test-page.html', '_blank'); undefined`);
    response = await dp.Target.onceTargetCreated();
    testRunner.log(response.params.targetInfo);
    var response = await dp.Target.onceTargetInfoChanged();
    testRunner.log(response.params.targetInfo);

    testRunner.completeTest();
  })