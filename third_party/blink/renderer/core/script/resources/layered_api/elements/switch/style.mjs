// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Style constant values.
const COLOR_ON = '#0077FF';
const TRACK_RADIUS = '13px';
const TRACK_BORDER_WIDTH = '2px';
const THUMB_HEIGHT = '22px';
const THUMB_WIDTH = '22px';
const THUMB_MARGIN_START = '2px';
const THUMB_MARGIN_END = '2px';

// Returns a function returning a CSSStyleSheet().
// TODO(tkent): Share this stylesheet factory feature with elements/toast/.
export function styleSheetFactory() {
  let styleSheet;
  return () => {
    if (!styleSheet) {
      styleSheet = new CSSStyleSheet();
      styleSheet.replaceSync(`
:host {
  block-size: 26px;
  border: none;
  box-sizing: border-box;
  display: inline-block;
  inline-size: 54px;
  user-select: none;
  vertical-align: middle;
}

#container {
  align-items: center;
  block-size: 100%;
  display: inline-flex;
  inline-size: 100%;
}

#thumb {
  background: white;
  block-size: ${THUMB_HEIGHT};
  border-radius: calc(${THUMB_HEIGHT} / 2);
  border: 1px solid black;
  box-sizing: border-box;
  display: inline-block;
  margin-inline-start: calc(-100% + ${THUMB_MARGIN_START});
  max-inline-size: ${THUMB_WIDTH};
  min-inline-size: ${THUMB_WIDTH};
}

#thumb.transitioning {
  transition: all linear 0.1s;
}

:host([on]) #thumb {
  border: 1px solid ${COLOR_ON};
  margin-inline-start: calc(0px - ${THUMB_WIDTH} - ${THUMB_MARGIN_END});
}

#track {
  block-size: 100%;
  border-radius: ${TRACK_RADIUS};
  border: ${TRACK_BORDER_WIDTH} solid #dddddd;
  box-shadow: 0 0 0 1px #f8f8f8;
  box-sizing: border-box;
  display: inline-block;
  inline-size: 100%;
  overflow: hidden;
  padding: 0px;
}

#trackFill {
  background: ${COLOR_ON};
  block-size: 100%;
  border-radius: calc(${TRACK_RADIUS}) - ${TRACK_BORDER_WIDTH});
  box-shadow: none;
  box-sizing: border-box;
  display: inline-block;
  inline-size: 0%;
  vertical-align: top;
}

#trackFill.transitioning {
  transition: all linear 0.1s;
}

:host([on]) #track {
  border: ${TRACK_BORDER_WIDTH} solid ${COLOR_ON};
}

:host(:focus) {
  outline-offset: 4px;
}

:host(:focus) #track {
  box-shadow: 0 0 0 2px #f8f8f8;
}

:host([on]:focus) #track {
  box-shadow: 0 0 0 2px #dddddd;
}

:host(:focus) #thumb {
  border: 2px solid black;
}

:host([on]:focus) #thumb {
  border: 2px solid ${COLOR_ON};
}

:host(:not(:focus-visible):focus) {
  outline: none;
}
`);
    }
    return styleSheet;
  };
}

/**
 * Add 'transitioning' class to the element, and remove it on 'transitionend'
 * event.
 *
 * TODO(tkent): This doesn't work well with customization by web authors because
 * class of a shadow element is invisible for web authors. We should apply
 * custom state.
 *
 * @param {!Element} element
 */
export function markTransition(element) {
  const CLASS_NAME = 'transitioning';
  element.classList.add(CLASS_NAME);
  let durations = element.computedStyleMap().getAll('transition-duration');
  if (!durations.some(duration => duration.value >= 0)) {
    // If the element will have no transitions, we remove CLASS_NAME
    // immediately.
    element.classList.remove(CLASS_NAME);
    return;
  }
  // If the element will have transitions, initialize counters and listeners
  // only once.
  if (element.runningTransitions !== undefined) {
    return;
  }
  element.runningTransitions = 0;
  element.addEventListener('transitionrun', e => {
    if (e.target === element) {
      ++element.runningTransitions;
    }
  });
  let handleEndOrCancel = e => {
    if (e.target === element && --element.runningTransitions === 0) {
      element.classList.remove(CLASS_NAME);
    }
  };
  element.addEventListener('transitionend', handleEndOrCancel);
  element.addEventListener('transitioncancel', handleEndOrCancel);
}
