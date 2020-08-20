// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertLastError = chrome.test.assertLastError;
var assertNoLastError = chrome.test.assertNoLastError;
var assertTrue = chrome.test.assertTrue;
var succeed = chrome.test.succeed;

const SEARCH_WORDS = 'search words';

// This is a split mode extension, but we only care about running the
// incognito portion.
if (chrome.extension.inIncognitoContext) {
  chrome.test.runTests([

    // Verify search results shown in specified incognito tab.
    function IncognitoSpecificTab() {
      chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
        testHelper(tabs, {search: SEARCH_WORDS, tabId: tabs[0].id});
      });
    },

    // Verify search results shown in current incognito tab.
    function IncognitoNoDisposition() {
      chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
        testHelper(tabs, {search: SEARCH_WORDS});
      });
    },
  ]);

  var testHelper = (tabs, queryInfo) => {
    assertEq(1, tabs.length);
    const tab = tabs[0];
    // The browser test should have spun up an incognito browser, which
    // should be active.
    assertTrue(tab.incognito);
    chrome.search.query(queryInfo, () => {
      assertNoLastError();
      chrome.tabs.query({windowId: tab.windowId}, (tabs) => {
        const tab = tabs[0];
        const url = new URL(tab.pendingUrl || tab.url);
        // The default search is google.
        assertEq('www.google.com', url.hostname);
        succeed();
      });
    });
  };
}