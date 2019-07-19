// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constant to indicate selection index is not set.
const NO_SELECT_INDEX = -1;

/**
 * Class to handle navigating text. Currently, only
 * navigation and selection in editable text fields is supported.
 */
class TextNavigationManager {
  /** @param {!NavigationManager} navigationManager */
  constructor(navigationManager) {
    /** @private {!NavigationManager} */
    this.navigationManager_ = navigationManager;

    /** @private {number} */
    this.selectionStartIndex_ = NO_SELECT_INDEX;

    /** @private {number} */
    this.selectionEndIndex_ = NO_SELECT_INDEX;

    /** @private {chrome.automation.AutomationNode} */
    this.selectionStartObject_;

    /** @private {chrome.automation.AutomationNode} */
    this.selectionEndObject_;
  }

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

  /**
   * TODO(rosalindag): Work on text selection functionality below.
   */

  /**
   * Get the currently selected node.
   * @private
   * @return {!chrome.automation.AutomationNode}
   */
  node_() {
    return this.navigationManager_.currentNode();
  }

  /**
   * Sets the selection using the selectionStart and selectionEnd
   * as the offset input for setDocumentSelection and the parameter
   * textNode as the object input for setDocumentSelection.
   * @private
   */
  setSelection_() {
    if (this.selectionStartIndex_ == NO_SELECT_INDEX ||
        this.selectionEndIndex_ == NO_SELECT_INDEX) {
      console.log('Selection bounds are not set properly.');
    } else {
      chrome.automation.setDocumentSelection({
        anchorObject: this.selectionStartObject_,
        anchorOffset: this.selectionStartIndex_,
        focusObject: this.selectionEndObject_,
        focusOffset: this.selectionEndIndex_
      });
    }
  }

  /**
   * Returns the selection start index.
   * @return {boolean}
   * @public
   */
  isSelStartIndexSet() {
    return this.selectionStartIndex_ !== NO_SELECT_INDEX;
  }
  /**
   * Returns either the selection start index or the selection end index of the
   * node based on the getStart param.
   * @param {!chrome.accessibilityPrivate.SyntheticKeyboardModifiers} node
   * @param {boolean} getStart
   * @return {number} selection start if getStart is true otherwise selection
   * end
   * @private
   */
  getSelectionIndexFromNode_(node, getStart) {
    let indexFromNode = NO_SELECT_INDEX;
    if (getStart) {
      indexFromNode = node.textSelStart;
    } else {
      indexFromNode = node.textSelEnd;
    }
    if (indexFromNode === undefined) {
      return NO_SELECT_INDEX;
    }
    return indexFromNode;
  }

  /**
   * Sets the selectionStart variable based on the selection of the current
   * node.
   * @public
   */
  setSelectStart() {
    this.selectionStartObject_ = this.node_();
    this.selectionStartIndex_ =
        this.getSelectionIndexFromNode_(this.selectionStartObject_, true);
  }

  /**
   * Sets the selectionEnd variable based on the selection of the current node.
   * @public
   */
  setSelectEnd() {
    this.selectionEndObject_ = this.node_();
    this.selectionEndIndex_ =
        this.getSelectionIndexFromNode_(this.selectionEndObject_, false);
    this.setSelection_();
  }
}
