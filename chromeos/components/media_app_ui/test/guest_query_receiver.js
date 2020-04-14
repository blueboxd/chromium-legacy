// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The last file list loaded into the guest, updated via a spy on loadFiles().
 * @type {?ReceivedFileList}
 */
let lastReceivedFileList = null;

/**
 * Test cases registered by GUEST_TEST.
 * @type {Map<string, function(): Promise<undefined>>}
 */
const guestTestCases = new Map();

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
  } else if (data.navigate !== undefined) {
    if (data.navigate === 'next') {
      lastReceivedFileList.loadNext();
      result = 'loadNext called';
    } else if (data.navigate === 'prev') {
      lastReceivedFileList.loadPrev();
      result = 'loadPrev called';
    } else {
      result = 'nothing called';
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
  } else if (data.renameLastFile) {
    try {
      const renameResult =
          await lastReceivedFileList.item(0).renameOriginalFile(
              data.renameLastFile);
      if (renameResult === RenameResult.FILE_EXISTS) {
        result = 'renameOriginalFile resolved file exists';
      } else {
        result = 'renameOriginalFile resolved success';
      }
    } catch (/** @type{Error} */ error) {
      result = `renameOriginalFile failed Error: ${error}`;
    }
  } else if (data.saveCopy) {
    const existingFile = lastReceivedFileList.item(0);
    if (!existingFile) {
      result = 'saveCopy failed, no file loaded';
    } else {
      DELEGATE.saveCopy(existingFile);
      result = 'boo yah!';
    }
  }
  return {testQueryResult: result};
}

/**
 * Acts on TestMessageRunTestCase.
 * @param {TestMessageRunTestCase} data
 * @return {!Promise<TestMessageResponseData>}
 */
async function runTestCase(data) {
  const testCase = guestTestCases.get(data.testCase);
  if (!testCase) {
    throw new Error(`Unknown test case: '${data.testCase}'`);
  }
  await testCase();  // Propate exceptions to the MessagePipe handler.
  return {testQueryResult: 'success'};
}

/**
 * Registers a test that runs in the guest context. To indicate failure, the
 * test throws an exception (e.g. via assertEquals).
 * @param {string} testName
 * @param {function(): Promise<undefined>} testCase
 */
function GUEST_TEST(testName, testCase) {
  guestTestCases.set(testName, testCase);
}

/**
 * Tells the test driver the guest test message handlers are installed. This
 * requires the test handler that receives the signal to be set up. The order
 * that this occurs can not be guaranteed. So this function retries until the
 * signal is handled, which requires the 'test-handlers-ready' handler to be
 * registered in driver.js.
 */
async function signalTestHandlersReady() {
  const EXPECTED_ERROR =
      `No handler registered for message type 'test-handlers-ready'`;
  while (true) {
    try {
      await parentMessagePipe.sendMessage('test-handlers-ready', {});
      return;
    } catch (/** @type {GenericErrorResponse} */ e) {
      if (e.message !== EXPECTED_ERROR) {
        console.error('Unexpected error in signalTestHandlersReady', e);
        return;
      }
    }
  }
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

  parentMessagePipe.registerHandler('run-test-case', (data) => {
    return runTestCase(/** @type{TestMessageRunTestCase} */ (data));
  });

  // Log errors, rather than send them to console.error. This allows the error
  // handling tests to work correctly, and is also required for
  // signalTestHandlersReady() to operate without failing tests.
  parentMessagePipe.logClientError = error =>
      console.log(JSON.stringify(error));

  // Install spies.
  const realLoadFiles = loadFiles;
  loadFiles = async (/** !ReceivedFileList */ fileList) => {
    lastReceivedFileList = fileList;
    return realLoadFiles(fileList);
  };
  signalTestHandlersReady();
}

// Ensure content and all scripts have loaded before installing test handlers.
if (document.readyState !== 'complete') {
  window.addEventListener('load', installTestHandlers);
} else {
  installTestHandlers();
}

//# sourceURL=guest_query_receiver.js
