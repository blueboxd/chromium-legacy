// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = async function() {
  // Waits 2 raf callbacks the notifies background page.
  await new Promise(window.requestAnimationFrame);
  await new Promise(window.requestAnimationFrame);

  chrome.extension.getBackgroundPage().onRaf(window);
}
