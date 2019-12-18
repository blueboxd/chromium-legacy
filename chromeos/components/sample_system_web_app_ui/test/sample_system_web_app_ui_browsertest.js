// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://sample-system-web-app.
 */

const HOST_ORIGIN = 'chrome://sample-system-web-app';

var SampleSystemWebAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }
};

// Tests that chrome://sample-system-web-app runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('SampleSystemWebAppUIBrowserTest', 'HasChromeSchemeURL', () => {
  const header = document.querySelector('h1');

  assertEquals(header.innerText, 'Sample System Web App');
  assertEquals(document.location.origin, HOST_ORIGIN);
});
