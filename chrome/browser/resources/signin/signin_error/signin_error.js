/* Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

(function() {
function initialize() {
  cr.addWebUIListener('clear-focus', clearFocus);

  // Prefer using |document.body.offsetHeight| instead of
  // |document.body.scrollHeight| as it returns the correct height of the
  // even when the page zoom in Chrome is different than 100%.
  chrome.send('initializedWithSize', [document.body.offsetHeight]);
}

function clearFocus() {
  document.activeElement.blur();
}

document.addEventListener('DOMContentLoaded', initialize);
})();
