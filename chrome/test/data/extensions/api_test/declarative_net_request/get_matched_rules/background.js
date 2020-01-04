// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Navigates to |url| and invokes |callback| when the navigation is complete.
function navigateTab(url, callback) {
  chrome.tabs.onUpdated.addListener(function updateCallback(_, info, tab) {
    if (info.status == 'complete' && tab.url == url) {
      chrome.tabs.onUpdated.removeListener(updateCallback);
      callback(tab);
    }
  });

  chrome.tabs.update({url: url});
}

var testServerPort;
function getServerURL(host) {
  if (!testServerPort)
    throw new Error('Called getServerURL outside of runTests.');
  return `http://${host}:${testServerPort}/`;
}

var tests = [function testGetMatchedRules() {
  const url = getServerURL('abc.com');

  navigateTab(url, (tab) => {
    chrome.declarativeNetRequest.getMatchedRules((details) => {
      chrome.test.assertTrue(!!details.rulesMatchedInfo);
      const matchedRules = details.rulesMatchedInfo;

      chrome.test.assertEq(1, matchedRules.length);
      const matchedRule = matchedRules[0];

      // The matched rule's timestamp's cannot be verified, but it should be
      // populated.
      chrome.test.assertTrue(matchedRule.hasOwnProperty('timeStamp'));
      delete matchedRule.timeStamp;

      const expectedRuleInfo = {
        rule: {ruleId: 1, sourceType: 'manifest'},
        tabId: tab.id
      };

      // Sanity check that the RulesMatchedInfo fields are populated
      // correctly.
      chrome.test.assertTrue(
          chrome.test.checkDeepEq(expectedRuleInfo, matchedRule));

      chrome.test.succeed();
    });
  });
}];

chrome.test.getConfig(function(config) {
  testServerPort = config.testServer.port;
  chrome.test.runTests(tests);
});
