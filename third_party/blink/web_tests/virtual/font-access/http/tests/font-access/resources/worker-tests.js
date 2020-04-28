'use strict';

promise_test(async t => {
  const worker = new Worker('resources/worker-font-access.js');
  await new Promise(resolve => {
    worker.onmessage = t.step_func(event => {
      assert_equals(event.data, 'ready', 'worker replies ready');
      resolve();
    });
  });

  await new Promise((resolve, reject) => {
    worker.onmessage = t.step_func(event => {
      const availableFonts = event.data;
      assert_true(availableFonts instanceof Array);
      assert_fonts_exist(availableFonts, getExpectedFontSet());
      resolve();
    });
    worker.postMessage('query');
  });
}, 'query(): available from dedicated worker');

promise_test(async t => {
  const worker = new SharedWorker('resources/worker-font-access.js');
  await new Promise(resolve => {
    worker.port.onmessage = t.step_func(event => {
      assert_equals(event.data, 'ready', 'worker replies ready');
      resolve();
    });
  });

  await new Promise((resolve, reject) => {
    worker.port.onmessage = t.step_func(event => {
      const availableFonts = event.data;
      assert_true(availableFonts instanceof Array);
      assert_fonts_exist(availableFonts, getExpectedFontSet());
      resolve();
    });
    worker.port.postMessage('query');
  });
}, 'query(): available from shared worker');
