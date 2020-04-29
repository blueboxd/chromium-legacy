'use strict';

function promiseWorkerReady(t, worker) {
  return new Promise((resolve, reject) => {
    let timeout;

    const readyFunc = t.step_func(event => {
      if (event.data.type != 'ready') {
        reject(event);
      }
      window.clearTimeout(timeout);
      resolve();
    });

    // Setting a timeout because the the worker
    // may not be ready to listen to the postMessage.
    timeout = t.step_timeout(() => {
      if ('port' in worker) {
        if (!worker.port.onmessage) {
          worker.port.onmessage = readyFunc;
        }
        worker.port.postMessage({action: 'ping'});
      } else {
        if (!worker.onmessage) {
          worker.onmessage = readyFunc;
        }
        worker.postMessage({action: 'ping'});
      }
    }, 10);
  });
}

function promiseHandleMessage(t, worker) {
  return new Promise((resolve, reject) => {
    const handler = t.step_func(event => {
      if (event.data.type === 'ready') {
        // Ignore: may received duplicate readiness messages.
        return;
      }

      if (event.data.type === 'error') {
        reject(event);
      }

      resolve(event);
    });

    if ('port' in worker) {
      worker.port.onmessage = handler;
    } else {
      worker.onmessage = handler;
    }
  });
}

promise_test(async t => {
  const worker = new Worker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);

  const promise = promiseHandleMessage(t, worker);
  worker.postMessage({action: 'query'});
  const event = await promise;

  assert_equals(event.data.type, 'query', "Response is of type query.");

  const localFonts = event.data.data;
  assert_true(Array.isArray(localFonts),
              `Response is of type Array. Instead got: ${localFonts}.`);
}, 'DedicatedWorker, query(): returns fonts');

promise_test(async t => {
  const worker = new Worker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);

  const promise = promiseHandleMessage(t, worker);
  worker.postMessage({
    action: 'getTables',
    options: {
      postscriptNameFilter: getExpectedFontSet().map(f => f.postscriptName),
    }
  });
  const event = await promise;

  assert_equals(event.data.type, 'getTables', 'Response is of type getTables.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, getExpectedFontSet());
  for (const f of localFonts) {
    assert_font_has_tables(f.postscriptName, f.tables, BASE_TABLES);
  }
}, 'DedicatedWorker, getTables(): all fonts have base tables that are not empty');

promise_test(async t => {
  const worker = new Worker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);

  const promise = promiseHandleMessage(t, worker);

  const tableQuery = ["cmap", "head"];
  worker.postMessage({
    action: 'getTables',
    options: {
      postscriptNameFilter: getExpectedFontSet().map(f => f.postscriptName),
      tables: tableQuery,
    }
  });

  const event = await promise;

  assert_equals(event.data.type, 'getTables', 'Response is of type getTables.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, getExpectedFontSet());
  for (const f of localFonts) {
    assert_font_has_tables(f.postscriptName, f.tables, tableQuery);
  }
}, 'DedicatedWorker, getTables([tableName,...]): returns tables');

promise_test(async t => {
  const worker = new SharedWorker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);

  const promise = promiseHandleMessage(t, worker);
  worker.port.postMessage({action: 'query'});
  const event = await promise;

  assert_equals(event.data.type, 'query', 'Response is of type query.');

  const localFonts = event.data.data;
  assert_true(Array.isArray(localFonts),
              `Response is of type Array. Instead got: ${localFonts}.`);
  assert_fonts_exist(localFonts, getExpectedFontSet());
}, 'SharedWorker, query(): returns fonts');

promise_test(async t => {
  const worker = new SharedWorker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);

  const promise = promiseHandleMessage(t, worker);

  worker.port.postMessage({
    action: 'getTables',
    options: {
      postscriptNameFilter: getExpectedFontSet().map(f => f.postscriptName),
    }
  });

  const event = await promise;

  assert_equals(event.data.type, 'getTables', 'Response is of type getTables.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, getExpectedFontSet());
  for (const f of localFonts) {
    assert_font_has_tables(f.postscriptName, f.tables, BASE_TABLES);
  }
}, 'SharedWorker, getTables(): all fonts have base tables that are not empty');

promise_test(async t => {
  const worker = new SharedWorker('resources/worker-font-access.js');
  await promiseWorkerReady(t, worker);

  const promise = promiseHandleMessage(t, worker);

  const tableQuery = ["cmap", "head"];
  worker.port.postMessage({
    action: 'getTables',
    options: {
      postscriptNameFilter: getExpectedFontSet().map(f => f.postscriptName),
      tables: tableQuery,
    }
  });

  const event = await promise;

  assert_equals(event.data.type, 'getTables', 'Response is of type getTables.');

  const localFonts = event.data.data;
  assert_fonts_exist(localFonts, getExpectedFontSet());
  for (const f of localFonts) {
    assert_font_has_tables(f.postscriptName, f.tables, tableQuery);
  }
}, 'SharedWorker, getTables([tableName,...]): returns tables that are not empty');
