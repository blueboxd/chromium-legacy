// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function getDisplayLanguage() {
      chrome.test.assertEq(
        '',
        chrome.accessibilityPrivate.getDisplayLanguage(''));
      chrome.test.assertEq(
        '',
        chrome.accessibilityPrivate.getDisplayLanguage('not a language code'));
      chrome.test.assertEq(
        'English',
        chrome.accessibilityPrivate.getDisplayLanguage('en'));
      chrome.test.assertEq(
        'English',
        chrome.accessibilityPrivate.getDisplayLanguage('en-US'));
      chrome.test.assertEq(
        'français',
        chrome.accessibilityPrivate.getDisplayLanguage('fr'));
      chrome.test.assertEq(
        '日本語',
        chrome.accessibilityPrivate.getDisplayLanguage('ja'));
      chrome.test.succeed();
  }
];
chrome.test.runTests(allTests);
