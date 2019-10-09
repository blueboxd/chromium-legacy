// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/** @return {!URL} */
function getRedirectUrl() {
  try {  // For potential TypeError from new URL().
    const urlString = new URL(location).searchParams.get('url');
    if (urlString) {
      const url = new URL(decodeURIComponent(urlString));
      // Perform basic filtering. Also skip 'dev-ui-loader' to avoid redirect
      // cycle (benign but bizarre). Note that |url.host| is always lowercase.
      if (url.protocol === 'chrome:' && url.host.match(/[a-z0-9_\-]+/) &&
          url.host !== 'dev-ui-loader') {
        return url;
      }
    }
  } catch (e) {
  }
  return new URL('chrome://chrome-urls');
}

function redirectToChromePage() {
  // Use replace() so the current page disappears from history.
  location.replace(getRedirectUrl());
}

function doInstall() {
  cr.sendWithPromise('installDevUiDfm').then((response) => {
    const status = response[0];
    if (status === 'success' || status === 'noop') {
      redirectToChromePage();
    } else if (status === 'failure') {
      document.querySelector('#failure-message').hidden = false;
    }
  });
}

document.addEventListener('DOMContentLoaded', () => {
  cr.sendWithPromise('getDevUiDfmState').then((state) => {
    if (state === 'installed') {
      redirectToChromePage();
    } else if (state === 'not-installed') {
      doInstall();
    }
  });
});
})();
