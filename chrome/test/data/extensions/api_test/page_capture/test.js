// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.extension.pageCapture.
// browser_tests.exe --gtest_filter=ExtensionPageCaptureApiTest.*

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const fail = chrome.test.callbackFail;
const pass = chrome.test.callbackPass;

var testUrl = 'http://www.a.com:PORT' +
    '/extensions/api_test/page_capture/google.html';

function waitForCurrentTabLoaded(callback) {
  chrome.tabs.getSelected(null, function(tab) {
    if (tab.status == 'complete') {
      callback();
      return;
    }
    window.setTimeout(function() {
      waitForCurrentTabLoaded(callback);
    }, 100);
  });
}

function testPageCapture(data, isFile) {
  assertEq(undefined, chrome.runtime.lastError);
  assertTrue(data != null);
  // It should contain few KBs of data.
  assertTrue(data.size > 100);
  var reader = new FileReader();
  // Let's make sure it contains some well known strings.
  reader.onload = function(e) {
    if (!isFile) {
      var text = e.target.result;
      assertTrue(text.indexOf(testUrl) != -1);
      assertTrue(text.indexOf('logo.png') != -1);
    }
    // Run the GC so the blob is deleted.
    window.setTimeout(function() {
      window.gc();
    });
    window.setTimeout(function() {
      chrome.test.notifyPass();
    }, 0);
  };
  reader.readAsText(data);
}

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);

  chrome.test.runTests([
    function saveAsMHTML() {
      chrome.tabs.getSelected(null, function(tab) {
        chrome.tabs.update(null, { "url": testUrl });
        waitForCurrentTabLoaded(pass(function() {
          chrome.pageCapture.saveAsMHTML(
              {'tabId': tab.id}, pass(function(data) {
                if (config.customArg == 'REQUEST_DENIED') {
                  chrome.test.assertLastError('User denied request.');
                  chrome.test.notifyPass();
                  return;
                }
                testPageCapture(data, false /* isFile */);
              }));
        }));
      });
    },

    function saveAsMHTMLFile() {
      chrome.tabs.getSelected(null, function(tab) {
        chrome.tabs.update(null, {'url': config.testDataDirectory});
        waitForCurrentTabLoaded(function() {
          if (config.customArg == 'REQUEST_DENIED') {
            chrome.pageCapture.saveAsMHTML(
                {'tabId': tab.id}, fail('User denied request.', function(data) {
                  chrome.test.assertLastError('User denied request.');
                  chrome.test.notifyPass();
                  return;
                }));
          } else if (config.customArg == 'ONLY_PAGE_CAPTURE_PERMISSION') {
            chrome.pageCapture.saveAsMHTML(
                {'tabId': tab.id},
                fail(
                    'Don\'t have permissions required to capture this page.',
                    function(data) {
                      chrome.test.assertLastError(
                          'Don\'t have permissions required to capture this ' +
                          'page.');
                      return;
                    }));
          } else {
            chrome.pageCapture.saveAsMHTML(
                {'tabId': tab.id}, pass(function(data) {
                  testPageCapture(data, true /* isFile */);
                }));
          }
        });
      });
    }
  ]);
});
