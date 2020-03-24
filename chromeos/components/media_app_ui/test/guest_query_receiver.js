// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The last file loaded into the guest, updated via a spy on loadFile().
 * @type {?Promise<!ReceivedFile>}
 */
let lastReceivedFile = null;

/**
 * Repeatedly runs a query selector until it finds an element.
 *
 * @param {string} query
 * @param {!Array<string>=} opt_path
 * @return {Promise<!Element>}
 */
async function waitForNode(query, opt_path) {
  /** @type {!HTMLElement|!ShadowRoot} */
  let node = document.body;
  const parent = opt_path ? opt_path.shift() : undefined;
  if (parent) {
    const element = await waitForNode(parent, opt_path);
    if (!(element instanceof HTMLElement) || !element.shadowRoot) {
      throw new Error('Path not a shadow root HTMLElement');
    }
    node = element.shadowRoot;
  }
  const existingElement = node.querySelector(query);
  if (existingElement) {
    return Promise.resolve(existingElement);
  }
  console.log('Waiting for ' + query);
  return new Promise(resolve => {
    const observer = new MutationObserver((mutationList, observer) => {
      const element = node.querySelector(query);
      if (element) {
        resolve(element);
        observer.disconnect();
      }
    });
    observer.observe(node, {childList: true, subtree: true});
  });
}

/**
 * Acts on received TestMessageQueryData.
 *
 * @param {TestMessageQueryData} data
 * @return {!Promise<TestMessageResponseData>}
 */
async function runTestQuery(data) {
  let result = 'no result';
  if (data.testQuery) {
    const element = await waitForNode(data.testQuery, data.pathToRoot || []);
    result = element.tagName;

    if (data.property) {
      result = JSON.stringify(element[data.property]);
    } else if (data.requestFullscreen) {
      try {
        await element.requestFullscreen();
        result = 'hooray';
      } catch (/** @type {TypeError} */ typeError) {
        result = typeError.message;
      }
    }
  } else if (data.overwriteLastFile) {
    const testBlob = new Blob([data.overwriteLastFile]);
    const ensureLoaded = await lastReceivedFile;
    await ensureLoaded.overwriteOriginal(testBlob);
    result = 'overwriteOriginal resolved';
  } else if (data.deleteLastFile) {
    try {
      const ensureLoaded = await lastReceivedFile;
      const deleteResult = await ensureLoaded.deleteOriginalFile();
      if (deleteResult === DeleteResult.FILE_MOVED) {
        result = 'deleteOriginalFile resolved file moved';
      } else {
        result = 'deleteOriginalFile resolved success';
      }
    } catch (/** @type{Error} */ error) {
      result = `deleteOriginalFile failed Error: ${error}`;
    }
  }

  return {testQueryResult: result};
}

// Wait until dom content has loaded to make sure receiver.js has been
// parsed and executed.
window.addEventListener('DOMContentLoaded', () => {
  parentMessagePipe.registerHandler('test', (data) => {
    return runTestQuery(/** @type {TestMessageQueryData} */ (data));
  });
  // Turn off error rethrowing for tests so the test runner doesn't mark
  // our error handling tests as failed.
  parentMessagePipe.rethrowErrors = false;
  // Handler that will always error for helping to test the message pipe
  // itself.
  parentMessagePipe.registerHandler('bad-handler', () => {
    throw Error('This is an error');
  });

  // Log errors, rather than send them to console.error.
  parentMessagePipe.logClientError = error =>
      console.log(JSON.stringify(error));

  // Install spies.
  const realLoadFile = loadFile;
  loadFile = async (/** number */ token, /** !File */ file) => {
    lastReceivedFile = realLoadFile(token, file);
    return lastReceivedFile;
  }
});

//# sourceURL=guest_query_receiver.js
