// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

chrome.test.runTests([
  function testTwoUpViewFeatureDisabled() {
    const toolbar = viewer.shadowRoot.querySelector('#zoom-toolbar');
    loadTimeData.overrideValues({'pdfTwoUpViewEnabled': false});
    toolbar.strings = Object.assign({}, toolbar.strings);

    const twoUpButton = toolbar.shadowRoot.querySelector('#two-up-view-button');
    chrome.test.assertTrue(!!twoUpButton);
    chrome.test.assertTrue(twoUpButton.hidden);

    const fitButton = toolbar.shadowRoot.querySelector('#fit-button');
    chrome.test.assertTrue(!!fitButton);
    chrome.test.assertEq(100, fitButton.delay);

    chrome.test.succeed();
  },


  function testTwoUpViewFeatureEnabled() {
    const toolbar = viewer.shadowRoot.querySelector('#zoom-toolbar');
    loadTimeData.overrideValues({'pdfTwoUpViewEnabled': true});
    toolbar.strings = Object.assign({}, toolbar.strings);

    const twoUpButton = toolbar.shadowRoot.querySelector('#two-up-view-button');
    chrome.test.assertTrue(!!twoUpButton);
    chrome.test.assertFalse(twoUpButton.hidden);

    const fitButton = toolbar.shadowRoot.querySelector('#fit-button');
    chrome.test.assertTrue(!!fitButton);
    chrome.test.assertEq(150, fitButton.delay);

    chrome.test.succeed();
  },
]);
