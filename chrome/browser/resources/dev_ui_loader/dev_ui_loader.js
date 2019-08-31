// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/** @param {string} page */
function redirectToChromePage(page) {
  // Use replace() so the current page disappears from history.
  location.replace('chrome://' + page);
}

function doInstall() {
  cr.sendWithPromise('installAndLoadDevUiDfm').then((response) => {
    const query = new URL(window.location).searchParams;
    const targetPage = (query.get('page') || '').replace(/[^a-z0-9_\-]/g, '');
    const status = response[0];
    if (status === 'failure') {
      document.querySelector('#failure-message').hidden = false;
    } else if (status === 'success' || status === 'noop') {
      redirectToChromePage(targetPage);
    }
  });
}

document.addEventListener('DOMContentLoaded', () => {
  cr.sendWithPromise('getDevUiDfmState').then((state) => {
    if (state === 'installed') {
      redirectToChromePage('chrome-urls');
    } else if (state === 'not-installed' || 'not-loaded') {
      doInstall();
    }
  });
});
})();
