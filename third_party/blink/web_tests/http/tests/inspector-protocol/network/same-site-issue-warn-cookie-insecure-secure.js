(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that setting a cookie cross-scheme triggers an inspector issue.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  await session.navigate('http://cookie.test:8000/inspector-protocol/resources/empty.html');

  const issuePromise = dp.Audits.onceIssueAdded();
  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value;');
  await session.evaluate(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);
  const issue = await issuePromise;
  // Clear request id.
  const replacer = (key, value) => {
    if (key === "requestId") return "<request-id>";
    return value;
  };
  testRunner.log(`Inspector issue: ${JSON.stringify(issue.params, replacer, 2)}\n`);

  testRunner.completeTest();
});
