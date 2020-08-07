// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {FittingType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/constants.js';
import {ViewerPdfToolbarNewElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer-pdf-toolbar-new.js';

/** @return {!ViewerPdfToolbarNewElement} */
function createToolbar() {
  document.body.innerHTML = '';
  const toolbar = /** @type {!ViewerPdfToolbarNewElement} */ (
      document.createElement('viewer-pdf-toolbar-new'));
  document.body.appendChild(toolbar);
  return toolbar;
}

/**
 * Returns the cr-icon-buttons in |toolbar|'s shadowRoot under |parentId|.
 * @param {!ViewerPdfToolbarNewElement} toolbar
 * @param {string} parentId
 * @return {!NodeList<!CrIconButtonElement>}
 */
function getCrIconButtons(toolbar, parentId) {
  return /** @type {!NodeList<!CrIconButtonElement>} */ (
      toolbar.shadowRoot.querySelectorAll(`#${parentId} cr-icon-button`));
}

// Unit tests for the viewer-pdf-toolbar-new element.
const tests = [
  /**
   * Test that the toolbar toggles between showing the fit-to-page and
   * fit-to-width buttons.
   */
  function testFitButton() {
    const toolbar = createToolbar();
    const fitButton = getCrIconButtons(toolbar, 'center')[2];
    const fitWidthIcon = 'pdf:fit-to-width';
    const fitHeightIcon = 'pdf:fit-to-height';

    let lastFitType = '';
    let numEvents = 0;
    toolbar.addEventListener('fit-to-changed', e => {
      lastFitType = e.detail;
      numEvents++;
    });

    // Initially FIT_TO_WIDTH, show FIT_TO_PAGE.
    chrome.test.assertEq(fitHeightIcon, fitButton.ironIcon);

    // Tap 1: Fire fit-to-changed(FIT_TO_PAGE), show fit-to-width.
    fitButton.click();
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, lastFitType);
    chrome.test.assertEq(1, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);

    // Tap 2: Fire fit-to-changed(FIT_TO_WIDTH), show fit-to-page.
    fitButton.click();
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, lastFitType);
    chrome.test.assertEq(2, numEvents);
    chrome.test.assertEq(fitHeightIcon, fitButton.ironIcon);

    // Do the same as above, but with fitToggle().
    toolbar.fitToggle();
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, lastFitType);
    chrome.test.assertEq(3, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);
    toolbar.fitToggle();
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, lastFitType);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitHeightIcon, fitButton.ironIcon);

    // Test forceFit(FIT_TO_PAGE): Updates the icon, does not fire an event.
    toolbar.forceFit(FittingType.FIT_TO_PAGE);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);

    // Force fitting the same fit as the existing fit should do nothing.
    toolbar.forceFit(FittingType.FIT_TO_PAGE);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);

    // Force fit width.
    toolbar.forceFit(FittingType.FIT_TO_WIDTH);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitHeightIcon, fitButton.ironIcon);

    // Force fit height.
    toolbar.forceFit(FittingType.FIT_TO_HEIGHT);
    chrome.test.assertEq(4, numEvents);
    chrome.test.assertEq(fitWidthIcon, fitButton.ironIcon);

    chrome.test.succeed();
  },

  function testZoomButtons() {
    const toolbar = createToolbar();

    let zoomInCount = 0;
    let zoomOutCount = 0;
    toolbar.addEventListener('zoom-in', () => zoomInCount++);
    toolbar.addEventListener('zoom-out', () => zoomOutCount++);

    const zoomButtons = getCrIconButtons(toolbar, 'zoom-controls');

    // Zoom out
    chrome.test.assertEq('pdf:remove', zoomButtons[0].ironIcon);
    zoomButtons[0].click();
    chrome.test.assertEq(0, zoomInCount);
    chrome.test.assertEq(1, zoomOutCount);

    // Zoom in
    chrome.test.assertEq('pdf:add', zoomButtons[1].ironIcon);
    zoomButtons[1].click();
    chrome.test.assertEq(1, zoomInCount);
    chrome.test.assertEq(1, zoomOutCount);

    chrome.test.succeed();
  },

  function testRotateButton() {
    const toolbar = createToolbar();
    const rotateButton = getCrIconButtons(toolbar, 'center')[3];
    chrome.test.assertEq('pdf:rotate-left', rotateButton.ironIcon);

    const promise = eventToPromise('rotate-left', toolbar);
    rotateButton.click();
    promise.then(() => chrome.test.succeed());
  },

  function testZoomField() {
    const toolbar = createToolbar();
    toolbar.viewportZoom = .8;
    const zoomField = toolbar.shadowRoot.querySelector('#zoom-controls input');
    chrome.test.assertEq('80%', zoomField.value);

    // Value is set based on viewport zoom.
    toolbar.viewportZoom = .533;
    chrome.test.assertEq('53%', zoomField.value);

    // Setting a new value sends the value in a zoom-changed event.
    let sentValue = -1;
    toolbar.addEventListener('zoom-changed', e => {
      sentValue = e.detail;
      chrome.test.assertEq(110, sentValue);
      chrome.test.succeed();
    });
    zoomField.value = '110%';
    zoomField.dispatchEvent(new CustomEvent('input'));
  },

  function testSinglePageView() {
    const toolbar = createToolbar();
    const singlePageViewButton =
        toolbar.shadowRoot.querySelector('#single-page-view-button');
    const twoPageViewButton =
        toolbar.shadowRoot.querySelector('#two-page-view-button');

    toolbar.addEventListener('two-up-view-changed', function(e) {
      chrome.test.assertEq(false, e.detail);
      chrome.test.assertEq(
          'true', singlePageViewButton.getAttribute('aria-checked'));
      chrome.test.assertFalse(
          singlePageViewButton.querySelector('iron-icon').hidden);
      chrome.test.assertEq(
          'false', twoPageViewButton.getAttribute('aria-checked'));
      chrome.test.assertTrue(
          twoPageViewButton.querySelector('iron-icon').hidden);
      chrome.test.succeed();
    });
    singlePageViewButton.click();
  },

  function testTwoPageView() {
    const toolbar = createToolbar();
    const singlePageViewButton =
        toolbar.shadowRoot.querySelector('#single-page-view-button');
    const twoPageViewButton =
        toolbar.shadowRoot.querySelector('#two-page-view-button');

    toolbar.addEventListener('two-up-view-changed', function(e) {
      chrome.test.assertEq(true, e.detail);
      chrome.test.assertEq(
          'true', twoPageViewButton.getAttribute('aria-checked'));
      chrome.test.assertFalse(
          twoPageViewButton.querySelector('iron-icon').hidden);
      chrome.test.assertEq(
          'false', singlePageViewButton.getAttribute('aria-checked'));
      chrome.test.assertTrue(
          singlePageViewButton.querySelector('iron-icon').hidden);
      chrome.test.succeed();
    });
    twoPageViewButton.click();
  },

  function testShowAnnotationsToggle() {
    const toolbar = createToolbar();

    const showAnnotationsButton =
        toolbar.shadowRoot.querySelector('#show-annotations-button');
    chrome.test.assertEq(
        'true', showAnnotationsButton.getAttribute('aria-checked'));
    chrome.test.assertFalse(
        showAnnotationsButton.querySelector('iron-icon').hidden);

    toolbar.addEventListener('display-annotations-changed', (e) => {
      chrome.test.assertEq(false, e.detail);
      chrome.test.assertEq(
          'false', showAnnotationsButton.getAttribute('aria-checked'));
      chrome.test.assertTrue(
          showAnnotationsButton.querySelector('iron-icon').hidden);
      chrome.test.succeed();
    });
    showAnnotationsButton.click();
  },

  function testSidenavToggleButton() {
    const toolbar = createToolbar();
    toolbar.addEventListener(
        'sidenav-toggle-click', () => chrome.test.succeed());
    const toggleButton = toolbar.shadowRoot.querySelector('#sidenavToggle');
    toggleButton.click();
  },
];

chrome.test.runTests(tests);
