// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides a mock of http://go/media-app/index.ts which is pre-built and
 * brought in via DEPS to ../../app/js/app_main.js. Runs in an isolated guest.
 */

/**
 * A mock app used for testing when src-internal is not available.
 * @implements mediaApp.ClientApi
 */
class BacklightApp extends HTMLElement {
  constructor() {
    super();
    this.lastImg =
        /** @type{!HTMLImageElement} */ (document.createElement('img'));
    this.appendChild(this.lastImg);
  }

  /** @override  */
  async loadFiles(files) {
    const img = /** @type{!HTMLImageElement} */ (document.createElement('img'));
    // Note the mock app will just leak this Blob URL.
    img.src = URL.createObjectURL(files.item(0).blob);
    await img.decode();

    // Simulate an app that shows one image at a time.
    this.replaceChild(img, this.lastImg);
    this.lastImg = img;
  }
}

window.customElements.define('backlight-app', BacklightApp);

document.addEventListener('DOMContentLoaded', () => {
  // The "real" app first loads translations for populating strings in the app
  // for the initial load, then does this.
  document.body.appendChild(new BacklightApp());
});
