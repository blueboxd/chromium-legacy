'use strict';

promise_test(async t => {
  const worker = new Worker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);
  const expectedFonts = getEnumerationTestSet({labelFilter: [TEST_SIZE_CATEGORY.small]});

  const promise = promiseHandleMessage(t, worker);
  worker.postMessage({
    action: 'blob',
    options: {
      postscriptNameFilter: expectedFonts.map(f => f.postscriptName),
    }
  });
  const event = await promise;

  assert_equals(event.data.type, 'blob', 'Response is of type blob.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, expectedFonts);
  for (const f of localFonts) {
    const parsedData = await parseFontData(f.blob);
    assert_font_has_tables(f.postscriptName, parsedData.tables, BASE_TABLES);
  }
}, 'DedicatedWorker, blob(): all fonts have base tables that are not empty');

promise_test(async t => {
  const worker = new SharedWorker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);
  const expectedFonts = getEnumerationTestSet({labelFilter: [TEST_SIZE_CATEGORY.small]});

  const promise = promiseHandleMessage(t, worker);
  worker.port.postMessage({
    action: 'blob',
    options: {
      postscriptNameFilter: expectedFonts.map(f => f.postscriptName),
    }
  });
  const event = await promise;

  assert_equals(event.data.type, 'blob', 'Response is of type blob.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, expectedFonts);
  for (const f of localFonts) {
    const parsedData = await parseFontData(f.blob);
    assert_font_has_tables(f.postscriptName, parsedData.tables, BASE_TABLES);
  }
}, 'SharedWorker, blob(): all fonts have base tables that are not empty');
