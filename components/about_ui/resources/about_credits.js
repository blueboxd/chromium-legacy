// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable no-restricted-properties */
function $(id) {
  return document.getElementById(id);
}
/* eslint-enable no-restricted-properties */

document.addEventListener('DOMContentLoaded', function() {
  if (cr.isChromeOS) {
    const keyboardUtils = document.createElement('script');

    const staticURLPolicy = trustedTypes.createPolicy('credits-static', {
      createScriptURL: () => {
        return 'chrome://credits/keyboard_utils.js';
      },
    });

    // TODO remove an empty string argument once supported
    // https://github.com/w3c/webappsec-trusted-types/issues/278
    keyboardUtils.src = staticURLPolicy.createScriptURL('');
    document.body.appendChild(keyboardUtils);
  }

  $('print-link').hidden = false;
  $('print-link').onclick = function() {
    window.print();
    return false;
  };

  document.addEventListener('keypress', function(e) {
    // Make the license show/hide toggle when the Enter is pressed.
    if (e.keyCode === 0x0d && e.target.tagName === 'LABEL') {
      e.target.previousElementSibling.click();
    }
  });
});
