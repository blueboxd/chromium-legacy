// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {oneGoogleBarApi} from './one_google_bar_api.js';

/**
 * The following |messageType|'s are sent to the parent frame:
 *  - loaded: sent on initial load.
 *  - overlaysUpdated: sent when an overlay is updated. The overlay bounding
 *        rects are included in the |data|.
 *  - activate/deactivate: When an overlay is open, 'activate' is sent to the
 *        to ntp-app so it can layer the OneGoogleBar over the NTP content. When
 *        no overlays are open, 'deactivate' is sent to ntp-app so the NTP
 *        content can be on top. The top bar of the OneGoogleBar is always on
 *        top.
 * @param {string} messageType
 * @param {Object} data
 */
function postMessage(messageType, data) {
  if (window === window.parent) {
    return;
  }
  window.parent.postMessage(
      {frameType: 'one-google-bar', messageType, data},
      'chrome://new-tab-page');
}

/**
 * Object that exposes:
 *  - |track()|: sets up MutationObserver to track element visibility changes.
 *  - |update(potentialNewOverlays)|: determines visibility of tracked elements
 *        and sends an update to the top frame about element visibility.
 * @type {!{
 *   track: !function(),
 *   update: !function(!Array<!Element>),
 * }}
 */
const overlayUpdater = (() => {
  const modalOverlays = document.documentElement.hasAttribute('modal-overlays');
  let shouldUndoDarkTheme = false;

  /** @type {!Set<!Element>} */
  const overlays = new Set();
  /** @type {!Array<!DOMRect>} */
  let lastOverlayRects = [];
  /** @type {number} */
  let elementsTransitioningCount = 0;
  /** @type {?number} */
  let updateIntervalId = null;
  /** @type {boolean} */
  let initialElementsAdded = false;

  const transitionStart = () => {
    elementsTransitioningCount++;
    if (!updateIntervalId) {
      updateIntervalId = setInterval(() => {
        update([]);
      });
    }
  };

  const transitionStop = () => {
    if (elementsTransitioningCount > 0) {
      elementsTransitioningCount--;
    }
    if (updateIntervalId && elementsTransitioningCount === 0) {
      clearInterval(updateIntervalId);
      updateIntervalId = null;
    }
  };

  /** @param {!Element} potentialNewOverlays */
  const addOverlay = overlay => {
    if (overlays.has(overlay)) {
      return;
    }
    // If an overlay starts a transition, the updated bounding rects need to
    // be sent to the top frame during the transition. The MutationObserver
    // will only handle new elements and changes to the element attributes.
    overlay.addEventListener('animationstart', transitionStart);
    overlay.addEventListener('animationend', transitionStop);
    overlay.addEventListener('animationcancel', transitionStop);
    overlay.addEventListener('transitionstart', transitionStart);
    overlay.addEventListener('transitionend', transitionStop);
    overlay.addEventListener('transitioncancel', transitionStop);
    // Update links that are loaded dynamically to ensure target is "_blank"
    // or "_top".
    // TODO(crbug.com/1039913): remove after OneGoogleBar links are updated.
    overlay.parentElement.querySelectorAll('a').forEach(el => {
      if (el.target !== '_blank' && el.target !== '_top') {
        el.target = '_top';
      }
    });
    if (!modalOverlays) {
      const {transition} = getComputedStyle(overlay);
      const opacityTransition = 'opacity 0.1s ease 0.02s';
      // Check if the transition is the default computed transition style. If it
      // is not, append to the existing transitions.
      if (transition === 'all 0s ease 0s') {
        overlay.style.transition = opacityTransition;
      } else if (!transition.includes('opacity')) {
        overlay.style.transition = transition + ', ' + opacityTransition;
      }
    }
    // The element has an initial opacity of 1. If the element is being added to
    // |overlays| and shown in the same |update()| call, the opacity transition
    // will not work since the opacity is already 1. For this reason the
    // 'fade-in' class is added to the element which runs an initial fade-in
    // animation.
    overlay.classList.add('fade-in');
    overlays.add(overlay);
  };

  /** @param {!Array<!Element>} potentialNewOverlays */
  const update = potentialNewOverlays => {
    const barRect = document.body.querySelector('#gb').getBoundingClientRect();
    if (barRect.bottom === 0) {
      return;
    }
    // After loaded, there could exist overlays that are shown, but not
    // mutated. Add all elements that could be an overlay. The children of the
    // actual overlay element are removed before sending any overlay update
    // message.
    if (!modalOverlays && !initialElementsAdded) {
      initialElementsAdded = true;
      Array.from(document.body.querySelectorAll('*')).forEach(el => {
        potentialNewOverlays.push(el);
      });
    }
    Array.from(potentialNewOverlays).forEach(overlay => {
      const rect = overlay.getBoundingClientRect();
      if (overlay.parentElement && rect.width > 0 &&
          rect.bottom > barRect.bottom) {
        addOverlay(overlay);
      }
    });
    // Remove overlays detached from DOM.
    Array.from(overlays).forEach(overlay => {
      if (!overlay.parentElement) {
        overlays.delete(overlay);
      }
    });
    // Check if an overlay and its parents are visible.
    const overlayRects = [];
    overlays.forEach(overlay => {
      const {display, visibility} = window.getComputedStyle(overlay);
      const rect = overlay.getBoundingClientRect();
      const shown = display !== 'none' && visibility !== 'hidden' &&
          rect.bottom > barRect.bottom;
      if (shown) {
        overlayRects.push(rect);
      }
      if (!modalOverlays) {
        // Setting the style here avoids triggering the mutation observer.
        overlay.style.opacity = shown ? '1' : '0';
      }
    });
    if (!modalOverlays) {
      overlayRects.push(barRect);
      const noChange = overlayRects.length === lastOverlayRects.length &&
          lastOverlayRects.every((rect, i) => {
            const newRect = overlayRects[i];
            return newRect.left === rect.left && newRect.top === rect.top &&
                newRect.right === rect.right && newRect.bottom === rect.bottom;
          });
      lastOverlayRects = overlayRects;
      if (noChange) {
        return;
      }
      postMessage('overlaysUpdated', overlayRects);
      return;
    }
    const overlayShown = overlayRects.length > 0;
    postMessage(overlayShown ? 'activate' : 'deactivate');
    // If the overlays are modal, a dark backdrop is displayed below the
    // OneGoogleBar iframe. The dark theme for the OneGoogleBar is then enabled
    // for better visibility.
    if (overlayShown) {
      if (!oneGoogleBarApi.isForegroundLight()) {
        shouldUndoDarkTheme = true;
        oneGoogleBarApi.setForegroundLight(true);
      }
    } else if (shouldUndoDarkTheme) {
      shouldUndoDarkTheme = false;
      oneGoogleBarApi.setForegroundLight(false);
    }
  };

  const track = () => {
    const observer = new MutationObserver(mutations => {
      const potentialNewOverlays = [];
      // Add any mutated element that is an overlay to |overlays|.
      mutations.forEach(({target}) => {
        if (overlays.has(target) || !target.parentElement) {
          return;
        }
        // When overlays are modal, the tooltips should not be treated like an
        // overlay.
        if (modalOverlays && target.parentElement.tagName === 'BODY') {
          return;
        }
        potentialNewOverlays.push(target);
      });
      update(potentialNewOverlays);
    });
    observer.observe(document.body, {
      attributes: true,
      childList: true,
      subtree: true,
    });
  };

  return {track, update};
})();

window.addEventListener('message', ({data}) => {
  if (data.type === 'enableDarkTheme') {
    oneGoogleBarApi.setForegroundLight(data.enabled);
  }
});

// Need to send overlay updates on resize because overlay bounding rects are
// absolutely positioned.
window.addEventListener('resize', () => {
  overlayUpdater.update([]);
});

// When the account overlay is shown, it does not close on blur. It does close
// when clicking the body.
window.addEventListener('blur', () => {
  document.body.click();
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
  overlayUpdater.track();
  oneGoogleBarApi.trackDarkModeChanges();
});
