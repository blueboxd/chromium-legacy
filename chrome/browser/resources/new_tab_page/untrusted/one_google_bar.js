// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const oneGoogleBarHeightInPixels = 64;

let darkThemeEnabled = false;
let shouldUndoDarkTheme = false;

/**
 * @param {boolean} enabled
 * @return {!Promise}
 */
async function enableDarkTheme(enabled) {
  if (!window.gbar) {
    return;
  }
  darkThemeEnabled = enabled;
  const ogb = await window.gbar.a.bf();
  ogb.pc.call(ogb, enabled ? 1 : 0);
}

/**
 * The following |messageType|'s are sent to the parent frame:
 *  - loaded: initial load
 *  - activate/deactivate: When an overlay is open, 'activate' is sent to the
 *        to ntp-app so it can layer the OneGoogleBar over the NTP content. When
 *        no overlays are open, 'deactivate' is sent to ntp-app so the NTP
 *        content can be on top. The top bar of the OneGoogleBar is always on
 *        top.
 * @param {string} messageType
 */
function postMessage(messageType) {
  if (window === window.parent) {
    return;
  }
  window.parent.postMessage(
      {frameType: 'one-google-bar', messageType}, 'chrome://new-tab-page');
}

function trackOverlayState() {
  const overlays = new Set();
  const observer = new MutationObserver(mutations => {
    // Add any mutated element that is an overlay to |overlays|.
    mutations.forEach(({target}) => {
      if (target.id === 'gb' || target.tagName === 'BODY' ||
          target.parentElement && target.parentElement.tagName === 'BODY') {
        return false;
      }
      if (target.offsetTop + target.offsetHeight > oneGoogleBarHeightInPixels) {
        overlays.add(target);
      }
    });
    // Remove overlays detached from DOM.
    Array.from(overlays).forEach(overlay => {
      if (!overlay.parentElement) {
        overlays.delete(overlay);
      }
    });
    // Check if an overlay and its parents are visible.
    const overlayShown = Array.from(overlays).some(overlay => {
      if (window.getComputedStyle(overlay).visibility === 'hidden') {
        return false;
      }
      let current = overlay;
      while (current) {
        if (window.getComputedStyle(current).display === 'none') {
          return false;
        }
        current = current.parentElement;
      }
      return true;
    });
    postMessage(overlayShown ? 'activate' : 'deactivate');
    // Allow the iframe z-level update to take effect before updating the
    // backdrop.
    setTimeout(() => {
      document.querySelector('#overlayBackdrop')
          .toggleAttribute('show', overlayShown);
      // When showing the backdrop, turn on dark theme for better visibility if
      // it is off.
      if (overlayShown && !darkThemeEnabled) {
        shouldUndoDarkTheme = true;
        enableDarkTheme(true);
      }
      if (!overlayShown && shouldUndoDarkTheme) {
        shouldUndoDarkTheme = false;
        enableDarkTheme(false);
      }
    });
  });
  observer.observe(
      document, {attributes: true, childList: true, subtree: true});
}

window.addEventListener('message', ({data}) => {
  if (data.type === 'enableDarkTheme') {
    enableDarkTheme(data.enabled);
  }
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
  trackOverlayState();
});
