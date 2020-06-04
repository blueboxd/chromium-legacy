'use strict';

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
  assert_fonts_exist(localFonts, getEnumerationTestSet());
}, 'DedicatedWorker, query(): returns fonts');

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
  assert_fonts_exist(localFonts, getEnumerationTestSet());
}, 'SharedWorker, query(): returns fonts');
