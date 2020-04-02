// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides a mock of http://go/media-app/index.ts which is pre-built and
 * brought in via DEPS to ../../app/js/app_main.js. Runs in an isolated guest.
 */

/**
 * Helper that returns UI that can serve as an effective mock of a fragment of
 * the real app, after loading a provided Blob URL.
 *
 * @typedef{function(string): Promise<!HTMLElement>}}
 */
let ModuleHandler;

/** @type{ModuleHandler} */
const createVideoChild = async (blobSrc) => {
  const container = /** @type{!HTMLElement} */ (
      document.createElement('backlight-video-container'));
  const video =
      /** @type{HTMLVideoElement} */ (document.createElement('video'));
  video.src = blobSrc;
  container.attachShadow({mode: 'open'});
  container.shadowRoot.appendChild(video);
  return container;
};

/** @type{ModuleHandler} */
const createImgChild = async (blobSrc) => {
  const img = /** @type{!HTMLImageElement} */ (document.createElement('img'));
  img.src = blobSrc;
  await img.decode();
  return img;
};

/**
 * A mock app used for testing when src-internal is not available.
 * @implements mediaApp.ClientApi
 */
class BacklightApp extends HTMLElement {
  constructor() {
    super();
    this.currentMedia =
        /** @type{!HTMLElement} */ (document.createElement('img'));
    this.appendChild(this.currentMedia);
  }

  /** @override  */
  async loadFiles(files) {
    const file = files.item(0);
    const isVideo = file.mimeType.match('^video/');
    // TODO(b/152832337): Remove this check when always using real image files
    // in tests. Image with size < 0 can't reliably be decoded. Don't apply the
    // size check to videos so MediaAppUIBrowserTest.CanFullscreenVideo doesn't
    // fail.
    if (file.size > 0 || isVideo) {
      const factory = isVideo ? createVideoChild : createImgChild;
      // Note the mock app will just leak this Blob URL.
      const child = await factory(URL.createObjectURL(file.blob));

      // Simulate an app that shows one image at a time.
      this.replaceChild(child, this.currentMedia);
      this.currentMedia = child;
    }
  }

  /** @override */
  setDelegate(delegate) {}
}
window.customElements.define('backlight-app', BacklightApp);

class VideoContainer extends HTMLElement {}
window.customElements.define('backlight-video-container', VideoContainer);

document.addEventListener('DOMContentLoaded', () => {
  // The "real" app first loads translations for populating strings in the app
  // for the initial load, then does this.
  document.body.appendChild(new BacklightApp());
});
