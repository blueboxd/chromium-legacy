// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const cannotCreateMultipleWindowsErrorMessage =
    'Can\'t create more than one window per extension.';
const cannotCloseNoWindowErrorMessage = 'No open window to close.';

const tests = {
  /* LoginScreenUi ************************************************************/
  'LoginScreenUiCanOpenWindow': () => {
    chrome.loginScreenUi.show({url: 'some/path.html'}, () => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  'LoginScreenUiCannotOpenMultipleWindows': () => {
    chrome.loginScreenUi.show({url: 'some/path.html'}, () => {
      chrome.test.assertNoLastError();
      chrome.loginScreenUi.show({url: 'some/path.html'}, () => {
        chrome.test.assertLastError(cannotCreateMultipleWindowsErrorMessage);
        chrome.test.succeed();
      });
    });
  },
  'LoginScreenUiCanOpenAndCloseWindow': () => {
    chrome.loginScreenUi.show({url: 'some/path.html'}, () => {
      chrome.test.assertNoLastError();
      chrome.loginScreenUi.close(() => {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
    });
  },
  'LoginScreenUiCannotCloseNoWindow': () => {
    chrome.loginScreenUi.close(() => {
      chrome.test.assertLastError(cannotCloseNoWindowErrorMessage);
      chrome.test.succeed();
    });
  },
};

// |waitForTestName()| waits for the browser test to reply with a test name and
// runs the specified test. The browser test logic can be found at
// chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.cc
function waitForTestName(testName) {
  if (!tests.hasOwnProperty(testName) ||
      typeof tests[testName] !== 'function') {
    chrome.test.fail('Test not found: ' + testName);
    return;
  }

  chrome.test.runTests([tests[testName]]);
}

chrome.test.sendMessage('Waiting for test name', waitForTestName);
