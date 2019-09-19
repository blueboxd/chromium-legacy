// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://media-app.
 */

GEN('#include "chromeos/constants/chromeos_features.h"');

const HOST_ORIGIN = 'chrome://media-app';
const GUEST_SRC = 'chrome://media-app-guest/app.html';

var MediaAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kMediaApp']};
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }
};

// Tests that chrome://media-app is allowed to frame chrome://media-app-guest.
// The URL is set in the html. If that URL can't load, test this fails like JS
// ERROR: "Refused to frame '...' because it violates the following Content
// Security Policy directive: "frame-src chrome://media-app-guest/".
// This test also fails if the guest renderer is terminated, e.g., due to webui
// performing bad IPC such as network requests (failure detected in
// content/public/test/no_renderer_crashes_assertion.cc).
TEST_F('MediaAppUIBrowserTest', 'GuestCanLoad', () => {
  const guest = document.querySelector('iframe');

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_SRC);
});

// TODO(crbug/996088): Add Tests that inspect the guest itself. Until the guest
// listens for postMessage calls, we can't test for anything interesting due to
// CORS (i.e. "Blocked a frame with origin chrome://media-app from accessing a
// cross-origin frame.").
