// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertLastError = chrome.test.assertLastError;
var assertNoLastError = chrome.test.assertNoLastError;
var assertTrue = chrome.test.assertTrue;
var succeed = chrome.test.succeed;

const SEARCH_WORDS = 'search words';

chrome.test.runTests([

  // Error if search string is empty.
  function QueryEmpty() {
    chrome.search.query({search: ''}, function() {
      assertLastError('Empty search parameter.');
      succeed();
    });
  },

  // Display results in current tab if no disposition is provided.
  function QueryPopulatedDispositionEmpty() {
    chrome.tabs.query({active: true}, (tabs) => {
      chrome.search.query({search: SEARCH_WORDS}, () => {
        didPerformQuery(tabs[0].id);
      });
    });
  },

  // Display results in current tab if said disposition is provided.
  function QueryPopulatedDispositionCurrentTab() {
    chrome.tabs.query({active: true}, (tabs) => {
      chrome.search.query(
          {search: SEARCH_WORDS, disposition: 'CURRENT_TAB'}, () => {
            didPerformQuery(tabs[0].id);
          });
    });
  },

  // Display results in new tab if said disposition is provided.
  function QueryPopulatedDispositionNewTab() {
    chrome.tabs.query({}, (initialTabs) => {
      let initialTabIds = initialTabs.map(tab => tab.id);
      chrome.search.query(
          {search: SEARCH_WORDS, disposition: 'NEW_TAB'}, () => {
            chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
              assertEq(1, tabs.length);
              // A new tab should have been created.
              assertFalse(initialTabIds.includes(tabs[0].id));
              didPerformQuery(tabs[0].id);
            });
          });
    });
  },

  // Display results in new window if said disposition is provided.
  function QueryPopulatedDispositionNewWindow() {
    chrome.windows.getAll({}, (initialWindows) => {
      let initialWindowIds = initialWindows.map(window => window.id);
      chrome.search.query(
          {search: SEARCH_WORDS, disposition: 'NEW_WINDOW'}, () => {
            chrome.windows.getAll({}, (windows) => {
              assertEq(windows.length, initialWindowIds.length + 1);
              let window =
                  windows.find(window => !initialWindowIds.includes(window.id));
              assertTrue(!!window);
              chrome.tabs.query({windowId: window.id}, (tabs) => {
                didPerformQuery(tabs[0].id);
              });
            });
          });
    });
  },

  // Display results in specified tab if said tabId is provided.
  function QueryPopulatedTabIDValid() {
    chrome.tabs.create({}, (tab) => {
      chrome.search.query({search: SEARCH_WORDS, tabId: tab.id}, () => {
        didPerformQuery(tab.id);
      });
    });
  },

  // Error if tab id invalid.
  function QueryPopulatedTabIDInvalid() {
    chrome.search.query({search: SEARCH_WORDS, tabId: -1}, () => {
      assertLastError('No tab with id: -1.');
      succeed();
    });
  },

  // Error if both tabId and Disposition populated.
  function QueryAndDispositionPopulatedTabIDValid() {
    chrome.tabs.query({active: true}, (tabs) => {
      chrome.search.query(
          {search: SEARCH_WORDS, tabId: tabs[0].id, disposition: 'NEW_TAB'},
          () => {
            assertLastError('Cannot set both \'disposition\' and \'tabId\'.');
            succeed();
          });
    });
  },
]);

var didPerformQuery = function(tabId) {
  assertNoLastError();
  chrome.tabs.get(tabId, (tab) => {
    const tabUrl = tab.pendingUrl || tab.url;
    const url = new URL(tabUrl);
    // The default search is google.
    assertEq('www.google.com', url.hostname);
    succeed();
  });
}