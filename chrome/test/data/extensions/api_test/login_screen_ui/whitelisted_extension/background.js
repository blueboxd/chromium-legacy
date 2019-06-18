// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cannotCreateMultipleWindowsErrorMessage =
  "Can't create more than one window per extension.";
var cannotCloseNoWindowErrorMessage = "No open window to close.";

var tests = {
  'CanOpenWindow': function CanOpenWindow() {
    chrome.loginScreenUi.show({url: 'some/path.html'}, function() {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  'CannotOpenMultipleWindows': function CannotOpenMultipleWindows() {
    chrome.loginScreenUi.show({url: 'some/path.html'}, function() {
      chrome.test.assertNoLastError();
      chrome.loginScreenUi.show({url: 'some/path.html'}, function() {
        chrome.test.assertLastError(cannotCreateMultipleWindowsErrorMessage);
        chrome.test.succeed();
      });
    });
  },
  'CanOpenAndCloseWindow': function CanOpenAndCloseWindow() {
    chrome.loginScreenUi.show({url: 'some/path.html'}, function() {
      chrome.test.assertNoLastError();
      chrome.loginScreenUi.close(function() {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
    });
  },
  'CannotCloseNoWindow': function CannotCloseNoWindow() {
    chrome.loginScreenUi.close(function() {
      chrome.test.assertLastError(cannotCloseNoWindowErrorMessage);
      chrome.test.succeed();
    });
  },
};

// |waitForTestName()| waits for the browser test to reply with a test name and
// runs the specified test. The browser test logic can be found at
// chrome/browser/chromeos/extensions/login_screen_ui/
// login_screen_extension_ui_handler_browsertest.cc
function waitForTestName(testName) {
  // No observer for NOTIFICATION_EXTENSION_TEST_MESSAGE or observer did not
  // reply with a test name.
  // This check is to prevent the API test under login_screen_ui_apitest.cc
  // from failing. This check can be removed once the API is stable and the API
  // test is no longer needed.
  if (testName === '') {
    return;
  }

  if (!tests.hasOwnProperty(testName) ||
    typeof tests[testName] !== 'function') {
    chrome.test.fail('Test not found: ' + testName);
    return;
  }

  chrome.test.runTests([tests[testName]]);
}

chrome.test.sendMessage('Waiting for test name', waitForTestName);
