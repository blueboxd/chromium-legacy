// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://telemetry-extension.
 */

GEN('#include "chromeos/components/telemetry_extension_ui/test/telemetry_extension_ui_browsertest.h"');

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

  /** @override */
  get typedefCppFixture() {
    return 'TelemetryExtensionUiBrowserTest';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get extraLibraries() {
    return [
      '//chromeos/components/telemetry_extension_ui/test/trusted_test_requester.js',
    ];
  }
};

// Tests that chrome://telemetry-extension runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('TelemetryExtensionUIBrowserTest', 'HasChromeSchemeURL', () => {
  const title = document.querySelector('title');

  expectEquals(title.innerText, 'Telemetry Extension');
  expectEquals(document.location.origin, HOST_ORIGIN);
  testDone();
});

// Tests that chrome://telemetry-extension embeds a
// chrome-untrusted:// iframe.
TEST_F('TelemetryExtensionUIBrowserTest', 'HasChromeUntrustedIframe', () => {
  const iframe = document.querySelector('iframe');
  expectTrue(iframe !== null);
  testDone();
});

// Test cases injected into the untrusted context.
// See implementations in untrusted_browsertest.js.

TEST_F(
    'TelemetryExtensionUIBrowserTest', 'UntrustedCanSpawnWorkers', async () => {
      expectTrue(await runTestInUntrusted('UntrustedCanSpawnWorkers'));
      testDone();
    });

TEST_F(
    'TelemetryExtensionUIBrowserTest', 'UntustedRequestTelemetryInfo',
    async () => {
      expectTrue(await runTestInUntrusted('UntustedRequestTelemetryInfo'));
      testDone();
    });
