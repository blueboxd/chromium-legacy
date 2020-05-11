(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that accessing a cookie in a breaking schemeful context downgrading situation triggers an inspector issue.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  await session.navigate('http://cookie.test:8000/inspector-protocol/resources/empty.html');

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=Strict');
  await session.evaluate(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);
  const issue = await dp.Audits.onceIssueAdded();
  // Clear request id.
  const replacer = (key, value) => {
    if (key === "requestId") return "<request-id>";
    return value;
  };
  testRunner.log(`Inspector issue: ${JSON.stringify(issue.params, replacer, 2)}\n`);

  testRunner.completeTest();
});
