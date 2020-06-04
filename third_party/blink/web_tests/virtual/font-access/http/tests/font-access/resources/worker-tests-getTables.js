'use strict';

promise_test(async t => {
  const worker = new Worker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);
  const expectedFonts = getEnumerationTestSet({labelFilter: [TEST_SIZE_CATEGORY.small]});

  const promise = promiseHandleMessage(t, worker);
  worker.postMessage({
    action: 'getTables',
    options: {
      postscriptNameFilter: expectedFonts.map(f => f.postscriptName),
    }
  });
  const event = await promise;

  assert_equals(event.data.type, 'getTables', 'Response is of type getTables.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, expectedFonts);
  for (const f of localFonts) {
    assert_font_has_tables(f.postscriptName, f.tables, BASE_TABLES);
  }
}, 'DedicatedWorker, getTables(): all fonts have base tables that are not empty');

promise_test(async t => {
  const worker = new Worker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);
  const expectedFonts = getEnumerationTestSet({labelFilter: [TEST_SIZE_CATEGORY.small]});

  const promise = promiseHandleMessage(t, worker);

  const tableQuery = ["cmap", "head"];
  worker.postMessage({
    action: 'getTables',
    options: {
      postscriptNameFilter: expectedFonts.map(f => f.postscriptName),
      tables: tableQuery,
    }
  });

  const event = await promise;

  assert_equals(event.data.type, 'getTables', 'Response is of type getTables.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, expectedFonts);
  for (const f of localFonts) {
    assert_font_has_tables(f.postscriptName, f.tables, tableQuery);
  }
}, 'DedicatedWorker, getTables([tableName,...]): returns tables');

promise_test(async t => {
  const worker = new SharedWorker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);
  const expectedFonts = getEnumerationTestSet({labelFilter: [TEST_SIZE_CATEGORY.small]});

  const promise = promiseHandleMessage(t, worker);

  worker.port.postMessage({
    action: 'getTables',
    options: {
      postscriptNameFilter: expectedFonts.map(f => f.postscriptName),
    }
  });

  const event = await promise;

  assert_equals(event.data.type, 'getTables', 'Response is of type getTables.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, expectedFonts);
  for (const f of localFonts) {
    assert_font_has_tables(f.postscriptName, f.tables, BASE_TABLES);
  }
}, 'SharedWorker, getTables(): all fonts have base tables that are not empty');

promise_test(async t => {
  const worker = new SharedWorker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);
  const expectedFonts = getEnumerationTestSet({labelFilter: [TEST_SIZE_CATEGORY.small]});

  const promise = promiseHandleMessage(t, worker);

  const tableQuery = ["cmap", "head"];
  worker.port.postMessage({
    action: 'getTables',
    options: {
      postscriptNameFilter: expectedFonts.map(f => f.postscriptName),
      tables: tableQuery,
    }
  });

  const event = await promise;

  assert_equals(event.data.type, 'getTables', 'Response is of type getTables.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, expectedFonts);
  for (const f of localFonts) {
    assert_font_has_tables(f.postscriptName, f.tables, tableQuery);
  }
}, 'SharedWorker, getTables([tableName,...]): returns tables that are not empty');
