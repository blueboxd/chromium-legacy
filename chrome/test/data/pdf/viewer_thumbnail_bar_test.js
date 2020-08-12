// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ViewerThumbnailBarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer-thumbnail-bar.js';
import {ViewerThumbnailElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer-thumbnail.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @return {!ViewerThumbnailBarElement} */
function createThumbnailBar() {
  document.body.innerHTML = '';
  const thumbnailBar = /** @type {!ViewerThumbnailBarElement} */ (
      document.createElement('viewer-thumbnail-bar'));
  document.body.appendChild(thumbnailBar);
  return thumbnailBar;
}

// Unit tests for the viewer-thumbnail-bar element.
const tests = [
  // Test that the thumbnail bar has the correct number of thumbnails.
  function testNumThumbnails() {
    const testDocLength = 10;
    const thumbnailBar = createThumbnailBar();
    thumbnailBar.docLength = testDocLength;

    flush();

    // Test that the correct number of viewer-thumbnail elements was created.
    const thumbnails =
        /** @type {!NodeList<!ViewerThumbnailElement>} */ (
            thumbnailBar.shadowRoot.querySelectorAll('viewer-thumbnail'));
    chrome.test.assertEq(testDocLength, thumbnails.length);

    // Test that each thumbnail has the correct page number.
    thumbnails.forEach((thumbnail, i) => {
      chrome.test.assertEq(i + 1, thumbnail.pageNumber);
    });

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
