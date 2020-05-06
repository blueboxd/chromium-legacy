// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('GestureCommandData');
goog.provide('GestureGranularity');

/**
 * Map from gesture names (ax::mojom::Gesture defined in
 *     ui/accessibility/ax_enums.mojom.)  to commands.
 * @type {!Object<string,
 *     {
 *     msgId: string,
 *     command: string,
 *     menuKeyOverride: ({keyCode: number, modifiers: ({ctrl:
 * boolean}|undefined)}|undefined),
 *     shouldRecoverRange: (boolean|undefined)
 *    }>}
 * @const
 */
GestureCommandData.GESTURE_COMMAND_MAP = {
  'click': {command: 'forceClickOnCurrentItem', msgId: 'click_gesture'},
  'swipeUp1': {
    msgId: 'swipeup1_gesture',
    command: 'previousAtGranularity',
    menuKeyOverride: {keyCode: 38 /* up */},
    shouldRecoverRange: true
  },
  'swipeDown1': {
    msgId: 'swipedown1_gesture',
    command: 'nextAtGranularity',
    menuKeyOverride: {keyCode: 40 /* Down */},
    shouldRecoverRange: true
  },
  'swipeLeft1': {
    msgId: 'swipeleft1_gesture',
    command: 'previousObject',
    menuKeyOverride: {keyCode: 37 /* left */},
    shouldRecoverRange: true
  },
  'swipeRight1': {
    msgId: 'swiperight1_gesture',
    command: 'nextObject',
    menuKeyOverride: {keyCode: 39 /* right */},
    shouldRecoverRange: true
  },
  'swipeUp2': {msgId: 'swipeup2_gesture', command: 'jumpToTop'},
  'swipeDown2': {msgId: 'swipedown2_gesture', command: 'readFromHere'},
  'swipeLeft2': {msgId: 'swipeleft2_gesture', command: 'previousWord'},
  'swipeRight2': {msgId: 'swiperight2_gesture', command: 'nextWord'},
  'swipeUp3': {msgId: 'swipeup3_gesture', command: 'scrollForward'},
  'swipeDown3': {msgId: 'swipedown3_gesture', command: 'scrollBackward'},
  'swipeLeft3': {msgId: 'swipeleft3_gesture', command: 'previousGranularity'},
  'swipeRight3': {msgId: 'swiperight3_gesture', command: 'nextGranularity'},

  'tap2': {msgId: 'tap2_gesture', command: 'stopSpeech'},
  'tap4': {msgId: 'tap4_gesture', command: 'showPanelMenuMostRecent'},
};

/**
 * Possible granularities to navigate.
 * @enum {number}
 */
GestureGranularity = {
  CHARACTER: 0,
  WORD: 1,
  LINE: 2,
  HEADING: 3,
  LINK: 4,
  FORM_FIELD_CONTROL: 5,
  COUNT: 6
};
