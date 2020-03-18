// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The following |messageType|'s are sent to the parent frame:
 *  - loaded: initial load
 *  - deactivate: the pointer is not over OneGoogleBar content
 *  - activate: the pointer is over OneGoogleBar content
 *
 * TODO(crbug.com/1039913): add support for light/dark theme. add support for
 *     forwarding touch events when OneGoogleBar is active.
 *
 * @param {string} messageType
 */
function postMessage(messageType) {
  window.parent.postMessage(
      {frameType: 'one-google-bar', messageType}, 'chrome://new-tab-page');
}

// Tracks if the OneGoogleBar is active and should accept pointer events.
let isActive;

/**
 * @param {number} x
 * @param {number} y
 */
function updateActiveState(x, y) {
  const shouldBeActive = document.elementFromPoint(x, y).tagName !== 'HTML';
  if (shouldBeActive === isActive) {
    return;
  }
  isActive = shouldBeActive;
  postMessage(shouldBeActive ? 'activate' : 'deactivate');
}

// Handle messages from parent frame which include forwarded mousemove events.
// The OneGoogleBar is loaded in an iframe on top of the embedder parent frame.
// The mousemove events are used to determine if the OneGoogleBar should be
// active or not.
// TODO(crbug.com/1039913): add support for touch which does not send mousemove
//                          events.
window.addEventListener('message', ({data}) => {
  if (data.type === 'mousemove') {
    updateActiveState(data.x, data.y);
  }
});

window.addEventListener('mousemove', ({x, y}) => {
  updateActiveState(x, y);
});

document.addEventListener('DOMContentLoaded', () => {
  // TODO(crbug.com/1039913): remove after OneGoogleBar links are updated.
  // Updates <a>'s so they load on the top frame instead of the iframe.
  document.body.querySelectorAll('a').forEach(el => {
    if (el.target !== '_blank') {
      el.target = '_top';
    }
  });
  postMessage('loaded');
});
