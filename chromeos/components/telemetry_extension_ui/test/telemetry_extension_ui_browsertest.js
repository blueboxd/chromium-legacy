// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://telemetry-extension.
 */

GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://telemetry-extension';
const UNTRUSTED_HOST_ORIGIN = 'chrome-untrusted://telemetry-extension';

var TelemetryExtensionUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }
};

// Tests that chrome://telemetry-extension runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('TelemetryExtensionUIBrowserTest', 'HasChromeSchemeURL', () => {
  const title = document.querySelector('title');

  assertEquals(title.innerText, 'Telemetry Extension');
  assertEquals(document.location.origin, HOST_ORIGIN);
});

// Tests that chrome://telemetry-extension embeds a
// chrome-untrusted:// iframe
TEST_F(
    'TelemetryExtensionUIBrowserTest', 'HasChromeUntrustedIframe', () => {
      const iframe = document.querySelector('iframe');
      window.onmessage = (event) => {
        assertEquals(event.origin, UNTRUSTED_HOST_ORIGIN);
        assertEquals(event.data.success, true);
        testDone();
      };
      iframe.contentWindow.postMessage('hello', UNTRUSTED_HOST_ORIGIN);
    });
