// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let driverSource = undefined;

/**
 * Test cases registered by GUEST_TEST.
 * @type {Map<string, function(): Promise<undefined>>}
 */
const guestTestCases = new Map();

async function actionQuery(/** MessageEvent<TestMessageQueryData> */ event) {
  switch (event.data.testQuery) {
    case 'run-test-case':
      await runTestCase(event.data.testData);
      window.parent.postMessage({testQueryResult: 'finished'}, '*');
      break;
    default:
      window.parent.postMessage(event.data.testQuery, 'chrome://help-app');
  }
}

/**
 * Acts on TestMessageRunTestCase. Logs any failures to console error.
 * @param {?string} testCaseName
 */
async function runTestCase(testCaseName) {
  if (!testCaseName) {
    console.error('No test case name given');
    return;
  }
  const testCase = guestTestCases.get(testCaseName);
  if (!testCase) {
    console.error(`Unknown test case: ${testCaseName}`);
    return;
  }
  try {
    await testCase();
  } catch (/** @type{Error} */ e) {
    console.error(`Failed: ${e.message}`);
  }
}

/**
 * Registers a test that runs in the guest context. To indicate failure, the
 * test logs a console error which fails these browser tests.
 * @param {string} testName
 * @param {function(): Promise<undefined>} testCase
 */
function GUEST_TEST(testName, testCase) {
  guestTestCases.set(testName, testCase);
}

function receiveTestMessage(/** Event */ e) {
  const event = /** @type{MessageEvent<TestMessageQueryData>} */ (e);
  if (event.data.testQuery) {
    // This is a message from the test driver. Action it.
    driverSource = event.source;
    actionQuery(event);
  } else if (driverSource) {
    // This is not a message from the test driver. Report it back to the driver.
    const response = {testQueryResult: event.data};
    driverSource.postMessage(response, '*');
  }
}

window.addEventListener('message', receiveTestMessage, false);

//# sourceURL=guest_query_reciever.js
