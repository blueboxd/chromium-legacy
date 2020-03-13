// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// |workerFactory| is given instead of workers, to create a worker after a
// test starts to avoid multiple workers are created at once.
function createWorker(workerFactory, onsuccess, onerror) {
  var worker = workerFactory();
  var onmessage = message => {
    if (worker.constructor === Worker) {
      worker.terminate();
    }
    // SharedWorker is terminated by close() in shared_worker.js.

    onsuccess(message.data);
  };
  if (worker.constructor === Worker) {
    worker.onmessage = onmessage;
  } else {
    worker.port.onmessage = onmessage;
  }
  worker.onerror = onerror;
}

function noRedirectTest(workerFactory, expectedUrl) {
  createWorker(
    workerFactory,
    workerUrl => {
      chrome.test.assertEq(expectedUrl, workerUrl);
      chrome.test.succeed();
    },
    () => { chrome.test.fail(); }
  );
}

function sameOriginRedirectTest(workerFactory, expectedUrl) {
  createWorker(
    workerFactory,
    workerUrl => {
      chrome.test.assertEq(expectedUrl, workerUrl);
      chrome.test.succeed();
    },
    () => { chrome.test.fail(); }
  );
}

function crossOriginRedirectTest(workerFactory) {
  createWorker(
    workerFactory,
    workerUrl => {
      chrome.test.fail('Cross-origin redirect of worker script should fail');
    },
    () => { chrome.test.succeed(); }
  );
}

chrome.test.getConfig(function(config) {
  const baseUrl = 'http://127.0.0.1:' + config.testServer.port;
  const crossOriginBaseUrl = 'http://localhost:' + config.testServer.port;

  const workerUrl = baseUrl + '/worker/worker.js';
  const sharedWorkerUrl = baseUrl + '/worker/shared_worker.js';

  const redirectedWorkerUrl = baseUrl + '/server-redirect?' + workerUrl;
  const redirectedSharedWorkerUrl =
    baseUrl + '/server-redirect?' + sharedWorkerUrl;

  const crossOriginRedirectedWorkerUrl =
    crossOriginBaseUrl + '/server-redirect?' + workerUrl;
  const crossOriginRedirectedSharedWorkerUrl =
    crossOriginBaseUrl + '/server-redirect?' + sharedWorkerUrl;

  chrome.test.runTests([
    noRedirectTest.bind(
      undefined,
      () => { return new Worker(workerUrl) },
      workerUrl),
    noRedirectTest.bind(
      undefined,
      () => { return new Worker(workerUrl, {type: 'module'}) },
      workerUrl),
    noRedirectTest.bind(
      undefined,
      () => {
        return new SharedWorker(
            sharedWorkerUrl,
            {name: 'noRedirectTest-classic'})
      },
      sharedWorkerUrl),
    noRedirectTest.bind(
      undefined,
      () => {
        return new SharedWorker(
            sharedWorkerUrl,
            {name: 'noRedirectTest-module', type: 'module'})
      },
      sharedWorkerUrl),

    sameOriginRedirectTest.bind(
      undefined,
      () => { return new Worker(redirectedWorkerUrl) },
      workerUrl),
    sameOriginRedirectTest.bind(
      undefined,
      () => { return new Worker(redirectedWorkerUrl, {type: 'module'}) },
      workerUrl),
    sameOriginRedirectTest.bind(
      undefined,
      () => {
        return new SharedWorker(
            redirectedSharedWorkerUrl,
            {name: 'sameOriginRedirectTest-classic'});
      },
      sharedWorkerUrl),
    sameOriginRedirectTest.bind(
      undefined,
      () => {
        return new SharedWorker(
            redirectedSharedWorkerUrl,
            {name: 'sameOriginRedirectTest-module', type: 'module'});
      },
      sharedWorkerUrl),

    crossOriginRedirectTest.bind(
      undefined,
      () => { return new Worker(crossOriginRedirectedWorkerUrl) }),
    crossOriginRedirectTest.bind(
      undefined,
      () => {
        return new Worker(crossOriginRedirectedWorkerUrl, {type: 'module'});
      }),
    crossOriginRedirectTest.bind(
      undefined,
      () => {
        return new SharedWorker(
            crossOriginRedirectedSharedWorkerUrl,
            {name: 'crossOriginRedirectTest-classic'});
      }),
    crossOriginRedirectTest.bind(
      undefined,
      () => {
        return new SharedWorker(
            crossOriginRedirectedSharedWorkerUrl,
            {name: 'crossOriginRedirectTest-module', type: 'module'});
      }),
  ]);
});
