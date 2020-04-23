(async function(testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
      `Verifies that mixed content issue is created from mixed content js within an oop iframe\n`);

    await dp.Network.enable();
    await dp.Audits.enable();
    await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

    dp.Target.onAttachedToTarget(async e => {
      const dp = session.createChild(e.params.sessionId).protocol;
      await dp.Network.enable();
      await dp.Audits.enable();
      const issuePromise = dp.Audits.onceIssueAdded();

      dp.Network.onRequestWillBeSent(async event => {
        if (event.params.request.url === 'http://example.test:8000/inspector-protocol/resources/blank.js') {
          const issue = await issuePromise;
          if (event.params.requestId !== issue?.params?.issue?.details?.mixedContentIssueDetails?.request?.requestId) {
            testRunner.log('FAIL: requestIds do not match');
          } else {
            testRunner.log('PASS: requestIds match');
          }
          if (event.params.frameId !== issue?.params?.issue?.details?.mixedContentIssueDetails?.frame?.frameId) {
            testRunner.log('FAIL: frameIds do not match');
          } else {
            testRunner.log('PASS: frameIds match');
          }
          testRunner.log(issue.params, "Inspector issue: ", ["frameId", "requestId"]);
          testRunner.completeTest();
        }
      });
    });

    await page.navigate('https://devtools.test:8443/inspector-protocol/resources/mixed-content-within-oopif.html');

    // test completion is ensured above in onRequestWillBeSent()
  })