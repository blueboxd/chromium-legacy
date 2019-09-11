// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;
const listeningUrlPattern = '*://cors.example.com/*';

function getCorsMode() {
  const query = location.search;
  const prefix = '?cors_mode=';
  chrome.test.assertTrue(query.startsWith(prefix));
  const mode = query.substr(prefix.length);
  chrome.test.assertTrue(mode == 'blink' || mode == 'network_service');
  return mode;
}

function registerOriginListeners(
    requiredNames, disallowedNames, extraInfoSpec) {
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
}

function registerRequestHeaderInjectionListeners(extraInfoSpec) {
  const beforeSendHeadersListener = callbackPass(details => {
    details.requestHeaders.push({name: 'x-foo', value: 'trigger-preflight'});
    return {requestHeaders: details.requestHeaders};
  });
  chrome.webRequest.onBeforeSendHeaders.addListener(
      beforeSendHeadersListener, {urls: [listeningUrlPattern]}, extraInfoSpec);

  // If the 'x-foo' header is injected by |beforeSendHeadersListener| without
  // 'extraHeaders' and with OOR-CORS being enabled, it triggers CORS
  // preflight, and the response for the preflight OPTIONS request is expected
  // to have the 'Access-Control-Allow-Headers: x-foo' header to pass the
  // security checks. Since the mock-http-headers for the target URL does not
  // provide the required header, the request fails in the CORS preflight.
  // Otherwises, modified headers are not observed by CORS implementations, and
  // do not trigger the CORS preflight.
  const triggerPreflight = !extraInfoSpec.includes('extraHeaders') &&
      getCorsMode() == 'network_service';

  const event = triggerPreflight ? chrome.webRequest.onErrorOccurred :
                                   chrome.webRequest.onCompleted;

  // Wait for the CORS request from the fetch.html to complete.
  const onCompletedOrErrorOccurredListener = callbackPass(details => {
    chrome.webRequest.onBeforeSendHeaders.removeListener(
        beforeSendHeadersListener);
    event.removeListener(onCompletedOrErrorOccurredListener);
  });
  event.addListener(
      onCompletedOrErrorOccurredListener, {urls: [listeningUrlPattern]});
}
function registerResponseHeaderInjectionListeners(extraInfoSpec) {
  const headersReceivedListener = details => {
    details.responseHeaders.push(
        {name: 'Access-Control-Allow-Origin', value: '*'});
    return { responseHeaders: details.responseHeaders };
  };
  chrome.webRequest.onHeadersReceived.addListener(
      headersReceivedListener, {urls: [listeningUrlPattern]}, extraInfoSpec);

  // If the 'extraHeaders' is not specified and OOR-CORS is enabled, Chrome
  // detects CORS failures before |headerReceivedListener| is called and injects
  // fake headers to deceive the CORS checks.
  const canInjectFakeCorsResponse =
      extraInfoSpec.includes('extraHeaders') || getCorsMode() == 'blink';

  const event = canInjectFakeCorsResponse ? chrome.webRequest.onCompleted :
                                            chrome.webRequest.onErrorOccurred;

  // Wait for the CORS request from the fetch.html to complete.
  const onCompletedOrErrorOccurredListener = callbackPass(details => {
    chrome.webRequest.onHeadersReceived.removeListener(headersReceivedListener);
    event.removeListener(onCompletedOrErrorOccurredListener);
  });
  event.addListener(
      onCompletedOrErrorOccurredListener, {urls: [listeningUrlPattern]});
}

runTests([
  function testOriginHeader() {
    // Register two sets of listener. One with extraHeaders and the second one
    // without it.
    // If OOR-CORS is enabled, the Origin header is invisible if the
    // extraHeaders is not specified.
    if (getCorsMode() == 'network_service')
      registerOriginListeners([], ['origin'], ['requestHeaders']);
    else
      registerOriginListeners(['origin'], [], ['requestHeaders']);
    registerOriginListeners(['origin'], [], ['requestHeaders', 'extraHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=accept'));
  },
  function testCorsSensitiveHeaderInjectionWithoutExtraHeaders() {
    registerRequestHeaderInjectionListeners(['blocking', 'requestHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=accept'));
  },
  function testCorsSensitiveHeaderInjectionWithExtraHeaders() {
    registerRequestHeaderInjectionListeners(
        ['blocking', 'requestHeaders', 'extraHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=accept'));
  },
  function testCorsResponseHeaderInjectionWithoutExtraHeaders() {
    registerResponseHeaderInjectionListeners(
        ['blocking', 'responseHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=reject'));
  },
  function testCorsResponseHeaderInjectionWithExtraHeaders() {
    registerResponseHeaderInjectionListeners(
        ['blocking', 'responseHeaders', 'extraHeaders']);

    // Wait for the navigation to complete.
    navigateAndWait(getServerURL(
        'extensions/api_test/webrequest/cors/fetch.html?path=reject'));
  },
]);
