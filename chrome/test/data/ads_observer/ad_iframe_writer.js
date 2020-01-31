// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates and iframe and appends it to the body element. Make sure the caller
// has a body element!
function createAdIframe() {
    let frame = document.createElement('iframe');
    document.body.appendChild(frame);
    return frame;
}

function createAdIframeWithSrc(src) {
  let frame = document.createElement('iframe');
  document.body.appendChild(frame);
  frame.src = src;
  return frame;
}
