// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ViewerPdfSidenavElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer-pdf-sidenav.js';

/** @return {!ViewerPdfSidenavElement} */
function createSidenav() {
  document.body.innerHTML = '';
  const sidenav = /** @type {!ViewerPdfSidenavElement} */ (
      document.createElement('viewer-pdf-sidenav'));
  document.body.appendChild(sidenav);
  return sidenav;
}

// Unit tests for the viewer-pdf-sidenav element.
const tests = [
  /**
   * Test that the sidenav toggles between outline and thumbnail view.
   */
  function testViewToggle() {
    const sidenav = createSidenav();
    const content = sidenav.shadowRoot.querySelector('#content');
    const buttons = /** @type {!NodeList<!CrIconButtonElement>} */ (
        sidenav.shadowRoot.querySelectorAll('cr-icon-button'));

    let visibleContent = content.querySelector('div:not([hidden])');
    chrome.test.assertTrue(
        buttons[0].parentNode.classList.contains('selected'));
    chrome.test.assertFalse(
        buttons[1].parentNode.classList.contains('selected'));
    chrome.test.assertEq('Thumbnails', visibleContent.textContent);

    // Click on outline view.
    buttons[1].click();
    visibleContent = content.querySelector('div:not([hidden])');
    chrome.test.assertFalse(
        buttons[0].parentNode.classList.contains('selected'));
    chrome.test.assertTrue(
        buttons[1].parentNode.classList.contains('selected'));
    chrome.test.assertEq('Outline', visibleContent.textContent);

    // Return to thumbnail view.
    buttons[0].click();
    visibleContent = content.querySelector('div:not([hidden])');
    chrome.test.assertTrue(
        buttons[0].parentNode.classList.contains('selected'));
    chrome.test.assertFalse(
        buttons[1].parentNode.classList.contains('selected'));
    chrome.test.assertEq('Thumbnails', visibleContent.textContent);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
