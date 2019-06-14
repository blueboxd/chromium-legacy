// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * List of URL hosts that can be requested by the webview.
 * @const {!Array<string>}
 */
const ALLOWED_HOSTS = [
  'families.google.com',
  'play.google.com',
  'google.com',
  'accounts.google.com',
  'gstatic.com',
  'fonts.gstatic.com',
  // FIFE avatar images (lh3-lh6). See http://go/fife-domains
  'lh3.googleusercontent.com',
  'lh4.googleusercontent.com',
  'lh5.googleusercontent.com',
  'lh6.googleusercontent.com',
];

/**
 * Returns whether the provided request should be allowed, based on whether
 * its URL matches the list of allowed hosts.
 * @param {!{url: string}} requestDetails Request that is issued by the webview.
 * @return {boolean} Whether the request should be allowed.
 */
function isAllowedRequest(requestDetails) {
  const requestUrl = new URL(requestDetails.url);
  return requestUrl.protocol == 'https' &&
      ALLOWED_HOSTS.includes(requestUrl.host);
}

let server = null;
const proxy = addSupervision.mojom.AddSupervisionHandler.getProxy();

document.addEventListener('DOMContentLoaded', () => {
  proxy.getOAuthToken().then((result) => {
    const webviewUrl = loadTimeData.getString('webviewUrl');
    if (!webviewUrl.startsWith('https://families.google.com')) {
      console.error('webviewUrl is not from https://families.google.com');
      return;
    }
    const eventOriginFilter = loadTimeData.getString('eventOriginFilter');
    const webview =
        /** @type {!WebView} */ (document.querySelector('#webview'));

    const accessToken = result.oauthToken;
    const flowType = loadTimeData.getString('flowType');
    const platformVersion = loadTimeData.getString('platformVersion');

    const url = new URL(webviewUrl);
    url.searchParams.set('flowType', flowType);
    url.searchParams.set('platformVersion', platformVersion);
    url.searchParams.set('accessToken', accessToken);

    // Allow guest webview content to open links in new windows.
    webview.addEventListener('newwindow', function(e) {
      window.open(e.targetUrl);
    });

    // Block any requests to URLs other than one specified
    // by eventOriginFilter.
    webview.request.onBeforeRequest.addListener(function(details) {
      return {cancel: !isAllowedRequest(details)};
    }, {urls: ['<all_urls>']}, ['blocking']);

    webview.src = url.toString();

    // Set up the server.
    server = new AddSupervisionAPIServer(webview, url, eventOriginFilter);
  });
});
