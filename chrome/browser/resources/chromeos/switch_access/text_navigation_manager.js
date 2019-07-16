// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle navigating text. Currently, only
 * navigation of editable text fields is supported.
 */
class TextNavigationManager {
  /**
   * Jumps to the beginning of the text field (does nothing
   * if already at the beginning).
   * @public
   */
  jumpToBeginning() {
    this.simulateKeyPress_(SAConstants.KeyCode.HOME, {ctrl: true});
  }

  /**
   * Jumps to the end of the text field (does nothing if
   * already at the end).
   * @public
   */
  jumpToEnd() {
    this.simulateKeyPress_(SAConstants.KeyCode.END, {ctrl: true});
  }

  /**
   * Moves the text caret one character back (does nothing
   * if there are no more characters preceding the current
   * location of the caret).
   * @public
   */
  moveBackwardOneChar() {
    this.simulateKeyPress_(SAConstants.KeyCode.LEFT_ARROW, {});
  }

  /**
   * Moves the text caret one word backwards (does nothing
   * if already at the beginning of the field). If the
   * text caret is in the middle of a word, moves the caret
   * to the beginning of that word.
   * @public
   */
  moveBackwardOneWord() {
    this.simulateKeyPress_(SAConstants.KeyCode.LEFT_ARROW, {ctrl: true});
  }

  /**
   * Moves the text caret one character forward (does nothing
   * if there are no more characters following the current
   * location of the caret).
   * @public
   */
  moveForwardOneChar() {
    this.simulateKeyPress_(SAConstants.KeyCode.RIGHT_ARROW, {});
  }

  /**
   * Moves the text caret one word forward (does nothing if
   * already at the end of the field). If the text caret is
   * in the middle of a word, moves the caret to the end of
   * that word.
   * @public
   */
  moveForwardOneWord() {
    this.simulateKeyPress_(SAConstants.KeyCode.RIGHT_ARROW, {ctrl: true});
  }

  /**
   * Moves the text caret one line up (does nothing
   * if there are no lines above the current location of
   * the caret).
   * @public
   */
  moveUpOneLine() {
    this.simulateKeyPress_(SAConstants.KeyCode.UP_ARROW, {});
  }

  /**
   * Moves the text caret one line down (does nothing
   * if there are no lines below the current location of
   * the caret).
   * @public
   */
  moveDownOneLine() {
    this.simulateKeyPress_(SAConstants.KeyCode.DOWN_ARROW, {});
  }

  /**
   * Simulates a single key stroke with the given key code
   * and keyboard modifiers (whether or not CTRL, ALT, SEARCH,
   * SHIFT are being held).
   *
   * @param {number} keyCode
   * @param {!chrome.accessibilityPrivate.SyntheticKeyboardModifiers} modifiers
   * @private
   */
  simulateKeyPress_(keyCode, modifiers) {
    chrome.accessibilityPrivate.sendSyntheticKeyEvent({
      type: chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyCode: keyCode,
      modifiers: modifiers
    });

    chrome.accessibilityPrivate.sendSyntheticKeyEvent({
      type: chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyCode: keyCode,
      modifiers: modifiers
    });
  }
}