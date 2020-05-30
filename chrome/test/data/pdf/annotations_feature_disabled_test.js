// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

chrome.test.runTests([
  function testAnnotationsDisabled() {
    const toolbar = document.body.querySelector('#toolbar');
    if (loadTimeData.getBoolean('pdfAnnotationsEnabled')) {
      chrome.test.assertTrue(!!toolbar.shadowRoot.querySelector('#annotate'));
    } else {
      chrome.test.assertFalse(!!toolbar.shadowRoot.querySelector('#annotate'));
    }
    chrome.test.succeed();
  },
  function testPromiseNotCreated() {
    chrome.test.assertEq(null, viewer.loaded);
    chrome.test.succeed();
  },
]);
