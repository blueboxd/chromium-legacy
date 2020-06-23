// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

chrome.test.runTests([
  function testPrintingEnabled() {
    loadTimeData.overrideValues({printingEnabled: true});
    const toolbar = viewer.shadowRoot.querySelector('#toolbar');
    // Trigger strings observer.
    toolbar.strings = Object.assign({}, toolbar.strings);
    const printIcon = toolbar.shadowRoot.querySelector('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertFalse(printIcon.hidden);
    chrome.test.succeed();
  },
  function testPrintingDisabled() {
    loadTimeData.overrideValues({printingEnabled: false});
    const toolbar = viewer.shadowRoot.querySelector('#toolbar');
    // Trigger strings observer.
    toolbar.strings = Object.assign({}, toolbar.strings);
    const printIcon = toolbar.shadowRoot.querySelector('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertTrue(printIcon.hidden);
    chrome.test.succeed();
  },
]);
