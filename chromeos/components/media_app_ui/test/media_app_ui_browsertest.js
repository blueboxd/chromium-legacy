// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://media-app.
 */

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "chromeos/components/media_app_ui/test/media_app_ui_browsertest.h"');

const HOST_ORIGIN = 'chrome://media-app';
const GUEST_ORIGIN = 'chrome://media-app-guest';

let driver = null;

var MediaAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//ui/webui/resources/js/assert.js',
      '//chromeos/components/media_app_ui/test/driver.js',
    ];
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kMediaApp']};
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get typedefCppFixture() {
    return 'MediaAppUiBrowserTest';
  }

  /** @override */
  setUp() {
    super.setUp();
    driver = new GuestDriver(GUEST_ORIGIN);
  }

  /** @override */
  tearDown() {
    driver.tearDown();
  }
};

// Tests that chrome://media-app is allowed to frame chrome://media-app-guest.
// The URL is set in the html. If that URL can't load, test this fails like JS
// ERROR: "Refused to frame '...' because it violates the following Content
// Security Policy directive: "frame-src chrome://media-app-guest/".
// This test also fails if the guest renderer is terminated, e.g., due to webui
// performing bad IPC such as network requests (failure detected in
// content/public/test/no_renderer_crashes_assertion.cc).
TEST_F('MediaAppUIBrowserTest', 'GuestCanLoad', async () => {
  const guest = document.querySelector('iframe');
  const app = await driver.waitForElementInGuest('backlight-app', 'tagName');

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_ORIGIN + '/app.html');
  assertEquals(app, '"BACKLIGHT-APP"');

  testDone();
});
