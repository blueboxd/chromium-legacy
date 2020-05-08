// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onImageLoad(img) {
  window.parent.postMessage(
      {
        frameType: 'background-image',
        messageType: 'loaded',
        src: img.src,
        time: Date.now(),
      },
      'chrome://new-tab-page');
}
