// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://telemetry-extension.
 */

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

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

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kTelemetryExtension']};
  }
};

// Tests that chrome://telemetry-extension runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('TelemetryExtensionUIBrowserTest', 'HasChromeSchemeURL', () => {
  const title = document.querySelector('title');

  assertEquals(title.innerText, 'Telemetry Extension');
  assertEquals(document.location.origin, HOST_ORIGIN);
});


var TelemetryExtensionUIUntrustedBrowserTest =
  class extends TelemetryExtensionUIBrowserTest {
    /** @override */
    get isAsync() {
      return true;
    }
  };

// Tests that chrome://telemetry-extension embeds a
// chrome-untrusted:// iframe
TEST_F('TelemetryExtensionUIUntrustedBrowserTest',
  'HasChromeUntrustedIframe', () => {
      const iframe = document.querySelector('iframe');
      window.onmessage = (event) => {
        assertEquals(event.origin, UNTRUSTED_HOST_ORIGIN);
        assertEquals(event.data.success, true);
        testDone();
      };
      iframe.contentWindow.postMessage('hello', UNTRUSTED_HOST_ORIGIN);
});

// Tests that chrome-untrusted:// can communicate with Worker.
TEST_F('TelemetryExtensionUIUntrustedBrowserTest',
  'HasChromeUntrustedWorker', () => {
  const iframe = document.querySelector('iframe');
  window.onmessage = (event) => {
    assertEquals(event.origin, UNTRUSTED_HOST_ORIGIN);
    assertEquals(event.data.message, 'WebWorker');
    testDone();
  };
  iframe.contentWindow.postMessage('runWebWorker', UNTRUSTED_HOST_ORIGIN);
});
