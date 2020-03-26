// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://media-app. */

// Test web workers can be spawned from chrome-untrusted://media-app. Errors
// will be logged in console from web_ui_browser_test.cc.
GUEST_TEST('GuestCanSpawnWorkers', () => {
  let error = null;

  try {
    const worker = new Worker('js/app_drop_target_module.js');
  } catch (e) {
    error = e;
  }

  assertEquals(error, null, error && error.message);
});
