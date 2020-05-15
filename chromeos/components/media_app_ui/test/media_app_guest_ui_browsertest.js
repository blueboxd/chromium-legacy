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

// Test that language is set correctly on the guest iframe.
GUEST_TEST('GuestHasLang', () => {
  assertEquals(document.documentElement.lang, 'en');
});

// Test can load files with CSP restrictions. We expect `error` to be called
// as these tests are loading resources that don't exist. Note: we can't violate
// CSP in tests or Js Errors will cause test failures.
GUEST_TEST('GuestCanLoadWithCspRestrictions', async () => {
  // Can load images served from chrome-untrusted://media-app/.
  const image = document.createElement('img');
  image.src = 'chrome-untrusted://media-app/does-not-exist.png';
  await test_util.eventToPromise('error', image);

  // Can load image data urls.
  const imageData = document.createElement('img');
  imageData.src = 'data:image/png;base64,iVBORw0KG';
  await test_util.eventToPromise('error', imageData);

  // Can load image blobs.
  const imageBlob = document.createElement('img');
  imageBlob.src = 'blob:chrome-untrusted://media-app/my-fake-blob-hash';
  await test_util.eventToPromise('error', imageBlob);

  // Can load video blobs.
  const videoBlob = document.createElement('video');
  videoBlob.src = 'blob:chrome-untrusted://media-app/my-fake-blob-hash';
  await test_util.eventToPromise('error', videoBlob);
});
