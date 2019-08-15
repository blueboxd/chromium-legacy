// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;
const listeningUrlPattern = '*://cors.example.com/*';

function testOriginHeader(requiredNames, disallowedNames, extraInfoSpec) {
  let observed = false;
  const beforeSendHeadersListener = callbackPass(details => {
    observed = true;
    checkHeaders(details.requestHeaders, requiredNames, disallowedNames);
    chrome.webRequest.onBeforeSendHeaders.removeListener(
        beforeSendHeadersListener);
  });
  chrome.webRequest.onBeforeSendHeaders.addListener(
      beforeSendHeadersListener, {urls: [listeningUrlPattern]}, extraInfoSpec);

  // Wait for the CORS request from the fetch.html to complete.
  const onCompletedListener = callbackPass(() => {
    chrome.test.assertTrue(observed);
    chrome.webRequest.onCompleted.removeListener(onCompletedListener);
  });
  chrome.webRequest.onCompleted.addListener(
      onCompletedListener, {urls: [listeningUrlPattern]});

  // Wait for the navigation to complete.
  navigateAndWait(
      getServerURL('extensions/api_test/webrequest/cors/fetch.html'));
}

runTests([
  function testOriginHeaderInvisible() {
    const requiredNames = [];
    const disallowedNames = ['origin'];
    const extraInfoSpec = ['requestHeaders'];
    testOriginHeader(requiredNames, disallowedNames, extraInfoSpec);
  },
  function testOriginHeaderVisible() {
    const requiredNames = ['origin'];
    const disallowedNames = [];
    const extraInfoSpec = ['requestHeaders', 'extraHeaders'];
    testOriginHeader(requiredNames, disallowedNames, extraInfoSpec);
  }
]);

