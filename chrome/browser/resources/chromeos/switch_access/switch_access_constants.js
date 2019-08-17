// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Constants used in Switch Access */
let SAConstants = {};

/**
 * The ID of the menu panel.
 * This must be kept in sync with the div ID in menu_panel.html.
 * @type {string}
 * @const
 */
SAConstants.MENU_PANEL_ID = 'switchaccess_menu_actions';

/**
 * IDs of menus that can appear in the menu panel.
 * This must be kept in sync with the div ID of each menu
 * in menu_panel.html.
 * @enum {string}
 * @const
 */
SAConstants.MenuId = {
  MAIN: 'main_menu',
  TEXT_NAVIGATION: 'text_navigation_menu'
};

/**
 * The ID of the back button.
 * This must be kept in sync with the back button ID in menu_panel.html.
 * @type {string}
 * @const
 */
SAConstants.BACK_ID = 'back';

SAConstants.Focus = {};

/**
 * The name of the focus class for the menu.
 * This must be kept in sync with the class name in menu_panel.css.
 * @type {string}
 * @const
 */
SAConstants.Focus.CLASS = 'focus';

/**
 * The ID used for the focus ring around the current element.
 * @type {string}
 * @const
 */
SAConstants.Focus.PRIMARY_ID = 'primary';

/**
 * The ID used for the focus ring around the current scope.
 * @type {string}
 * @const
 */
SAConstants.Focus.SCOPE_ID = 'scope';

/**
 * The buffer (in dip) between the primary focus ring and the scope focus ring.
 * @type {number}
 * @const
 */
SAConstants.Focus.SCOPE_BUFFER = 2;

/**
 * The ID used for the focus ring around the active text input.
 * @type {string}
 * @const
 */
SAConstants.Focus.TEXT_ID = 'text';

/**
 * The inner color of the focus rings.
 * @type {string}
 * @const
 */
SAConstants.Focus.PRIMARY_COLOR = '#1A73E8FF';

/**
 * The outer color of the focus rings.
 * @type {string}
 * @const
 */
SAConstants.Focus.SECONDARY_COLOR = '#0006';

/**
 * The amount of space (in px) needed to fit a focus ring around an element.
 * @type {number}
 * @const
 */
SAConstants.Focus.BUFFER = 4;

/**
 * The delay between keydown and keyup events on the virtual keyboard,
 * allowing the key press animation to display.
 * @type {number}
 * @const
 */
SAConstants.KEY_PRESS_DURATION_MS = 100;

/**
 * Preferences that are configurable in Switch Access.
 * @enum {string}
 */
SAConstants.Preference = {
  AUTO_SCAN_ENABLED: 'settings.a11y.switch_access.auto_scan.enabled',
  AUTO_SCAN_TIME: 'settings.a11y.switch_access.auto_scan.speed_ms',
  AUTO_SCAN_KEYBOARD_TIME:
      'settings.a11y.switch_access.auto_scan.keyboard.speed_ms'
};

/**
 * Actions available in the Switch Access Menu.
 * @enum {string}
 * @const
 */
SAConstants.MenuAction = {
  // Copy text.
  COPY: 'copy',
  // Cut text.
  CUT: 'cut',
  // Decrement the value of an input field.
  DECREMENT: chrome.automation.ActionType.DECREMENT,
  // Activate dictation for voice input to an editable text region.
  DICTATION: 'dictation',
  // Increment the value of an input field.
  INCREMENT: chrome.automation.ActionType.INCREMENT,
  // Move text caret to the beginning of the text field.
  JUMP_TO_BEGINNING_OF_TEXT: 'jumpToBeginningOfText',
  // Move text caret to the end of the text field.
  JUMP_TO_END_OF_TEXT: 'jumpToEndOfText',
  // Open and jump to the virtual keyboard
  KEYBOARD: 'keyboard',
  // Move text caret one character backward.
  MOVE_BACKWARD_ONE_CHAR_OF_TEXT: 'moveBackwardOneCharOfText',
  // Move text caret one word backward.
  MOVE_BACKWARD_ONE_WORD_OF_TEXT: 'moveBackwardOneWordOfText',
  // Move text caret one line down.
  MOVE_DOWN_ONE_LINE_OF_TEXT: 'moveDownOneLineOfText',
  // Move text caret one character forward.
  MOVE_FORWARD_ONE_CHAR_OF_TEXT: 'moveForwardOneCharOfText',
  // Move text caret one word forward.
  MOVE_FORWARD_ONE_WORD_OF_TEXT: 'moveForwardOneWordOfText',
  // Move text caret one line up.
  MOVE_UP_ONE_LINE_OF_TEXT: 'moveUpOneLineOfText',
  // Paste text.
  PASTE: 'paste',
  // Scroll the current element (or its ancestor) logically backwards.
  // Primarily used by ARC++ apps.
  SCROLL_BACKWARD: chrome.automation.ActionType.SCROLL_BACKWARD,
  // Scroll the current element (or its ancestor) down.
  SCROLL_DOWN: chrome.automation.ActionType.SCROLL_DOWN,
  // Scroll the current element (or is ancestor) logically forwards.
  // Primarily used by ARC++ apps.
  SCROLL_FORWARD: chrome.automation.ActionType.SCROLL_FORWARD,
  // Scroll the current element (or its ancestor) left.
  SCROLL_LEFT: chrome.automation.ActionType.SCROLL_LEFT,
  // Scroll the current element (or its ancestor) right.
  SCROLL_RIGHT: chrome.automation.ActionType.SCROLL_RIGHT,
  // Scroll the current element (or its ancestor) up.
  SCROLL_UP: chrome.automation.ActionType.SCROLL_UP,
  // Either perform the default action or enter a new scope, as applicable.
  SELECT: 'select',
  // Open and jump to the Switch Access settings.
  SETTINGS: 'settings',
  // Show the system context menu for the current element.
  SHOW_CONTEXT_MENU: chrome.automation.ActionType.SHOW_CONTEXT_MENU,
  // Set the end of a text selection.
  SELECT_END: 'selectEnd',
  // Set the beginning of a text selection.
  SELECT_START: 'selectStart'
};

/**
 * Empty location, used for hiding the menu.
 * @const
 */
SAConstants.EMPTY_LOCATION = {
  left: 0,
  top: 0,
  width: 0,
  height: 0
};

/**
 * Defines the key codes of all key events to be sent.
 * Currently used for text navigation actions and cut/copy/paste.
 * @enum {number}
 * @const
 */
SAConstants.KeyCode = {
  END: 35,
  HOME: 36,
  LEFT_ARROW: 37,
  UP_ARROW: 38,
  RIGHT_ARROW: 39,
  DOWN_ARROW: 40,
  // Key codes for X, C, V used for cut, copy, and paste synthetic key events.
  C: 67,
  V: 86,
  X: 88
};
