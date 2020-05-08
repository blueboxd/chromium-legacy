(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests we properly emit Page.downloadWillBegin.');

  dp.Page.onDownloadWillBegin(event => {
    testRunner.log(event);
  });

  async function waitForDownloadAndDump() {
    const visitedStates = new Set();
    await new Promise(resolve => {
      dp.Page.onDownloadProgress(event => {
        if (visitedStates.has(event.params.state))
          return;
        visitedStates.add(event.params.state);
        testRunner.log(event);
        if (event.params.state === 'completed')
          resolve();
      });
    });
  }

  await dp.Page.enable();
  testRunner.log('Downloading via a navigation: ');
  session.evaluate('location.href = "/devtools/network/resources/resource.php?download=1"');
  await waitForDownloadAndDump();
  testRunner.log('Downloading by clicking a link: ');
  session.evaluate(`
    const a = document.createElement('a');
    a.href = '/devtools/network/resources/resource.php';
    a.download = 'hello.text';
    document.body.appendChild(a);
    a.click();
  `);
  await waitForDownloadAndDump();
  testRunner.log(
      'Downloading by clicking a link (no HTTP Content-Disposition header, a[download=""]): ');
  session.evaluate(`
    const blankDownloadAttr = document.createElement('a');
    blankDownloadAttr.href = '/devtools/network/resources/download.zzz';
    blankDownloadAttr.download = ''; // Intentionally left blank to trigger unamed download
    document.body.appendChild(blankDownloadAttr);
    blankDownloadAttr.click();
  `);
  await waitForDownloadAndDump();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header with filename=foo.txt, no a[download]): ');
  session.evaluate(`
    const headerButNoDownloadAttr = document.createElement('a');
    headerButNoDownloadAttr.href = '/devtools/network/resources/resource.php?named_download=foo.txt';
    document.body.appendChild(headerButNoDownloadAttr);
    headerButNoDownloadAttr.click();
  `);
  await waitForDownloadAndDump();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header with filename=override.txt, a[download="foo.txt"]): ');
  session.evaluate(`
    const headerAndConflictingDownloadAttr = document.createElement('a');
    headerAndConflictingDownloadAttr.href = '/devtools/network/resources/resource.php?named_download=override.txt';
    headerAndConflictingDownloadAttr.download = 'foo.txt';
    document.body.appendChild(headerAndConflictingDownloadAttr);
    headerAndConflictingDownloadAttr.click();
  `);
  await waitForDownloadAndDump();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header without filename, no a[download]): ');
  session.evaluate(`
    const unnamedDownload = document.createElement('a');
    unnamedDownload.href = '/devtools/network/resources/resource.php?named_download';
    document.body.appendChild(unnamedDownload);
    unnamedDownload.click();
  `);
  await waitForDownloadAndDump();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header without filename, no a[download], js): ');
  session.evaluate(`
    const jsDownload = document.createElement('a');
    jsDownload.href = '/devtools/network/resources/resource.php?named_download&type=js';
    document.body.appendChild(jsDownload);
    jsDownload.click();
  `);
  await waitForDownloadAndDump();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header without filename, no a[download], image): ');
  session.evaluate(`
    const imageDownload = document.createElement('a');
    imageDownload.href = '/devtools/network/resources/resource.php?named_download&type=image';
    document.body.appendChild(imageDownload);
    imageDownload.click();
  `);
  await waitForDownloadAndDump();
  testRunner.completeTest();
})
