// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

window.googleAuth = null;
window.googleAuthPromise = new Promise((resolve, reject) => {
  googleAuthPromiseResolve = resolve;
});

let _googleAuthPromiseResolve = null;
function handleClientLoad() {
  if (requiresAuthentication()) {
    gapi.load('client:auth2', initClient);
  }
}

function initClient() {
  return gapi.client.init({
      'apiKey': AUTH_API_KEY,
      'clientId': AUTH_CLIENT_ID,
      'discoveryDocs': [AUTH_DISCOVERY_URL],
      'scope': AUTH_SCOPE,
  }).then(function () {
    window.googleAuth = gapi.auth2.getAuthInstance();
    if (!window.googleAuth.isSignedIn.get()) {
      window.googleAuth.signIn().then(setSigninStatus);
    } else {
      setSigninStatus();
    }
  });
}

function setSigninStatus() {
  let user = window.googleAuth.currentUser.get();
  _googleAuthPromiseResolve(user.getAuthResponse());
}

function requiresAuthentication() {
  let urlParams = new URLSearchParams(window.location.search);
  return !!urlParams.get('authenticate');
}
