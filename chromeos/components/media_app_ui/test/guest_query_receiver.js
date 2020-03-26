// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The last file list loaded into the guest, updated via a spy on loadFiles().
 * @type {?ReceivedFileList}
 */
let lastReceivedFileList = null;

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
    await lastReceivedFileList.item(0).overwriteOriginal(testBlob);
    result = 'overwriteOriginal resolved';
  } else if (data.deleteLastFile) {
    try {
      const deleteResult =
          await lastReceivedFileList.item(0).deleteOriginalFile();
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

function installTestHandlers() {
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

  // Log errors, rather than sending them to console.error.
  parentMessagePipe.logClientError = error =>
      console.log(JSON.stringify(error));

  // Install spies.
  const realLoadFiles = loadFiles;
  loadFiles = async (/** !ReceivedFileList */ fileList) => {
    lastReceivedFileList = fileList;
    realLoadFiles(fileList);
  }
}

// Ensure content and all scripts have loaded before installing test handlers.
if (document.readyState !== 'complete') {
  window.addEventListener('load', installTestHandlers);
} else {
  installTestHandlers();
}

//# sourceURL=guest_query_receiver.js
