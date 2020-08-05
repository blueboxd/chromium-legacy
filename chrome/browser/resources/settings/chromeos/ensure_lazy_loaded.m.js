// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let lazyLoadPromise = null;

/** @return {!Promise<void>} Resolves when the lazy load module is imported. */
export function ensureLazyLoadedOs() {
  if (!lazyLoadPromise) {
    const script = document.createElement('script');
    script.type = 'module';
    script.src = './lazy_load.js';
    document.body.appendChild(script);

    lazyLoadPromise = Promise.all([
      'os-settings-powerwash-dialog',
      'os-settings-privacy-page',
      'os-settings-reset-page',
    ].map(name => customElements.whenDefined(name)));
  }
  return lazyLoadPromise;
}
