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
    this.navigationManager_.simulateKeyPress(
        SAConstants.KeyCode.HOME, {ctrl: true});
  }

  /**
   * Jumps to the end of the text field (does nothing if
   * already at the end).
   * @public
   */
  jumpToEnd() {
    this.navigationManager_.simulateKeyPress(
        SAConstants.KeyCode.END, {ctrl: true});
  }

  /**
   * Moves the text caret one character back (does nothing
   * if there are no more characters preceding the current
   * location of the caret).
   * @public
   */
  moveBackwardOneChar() {
    this.navigationManager_.simulateKeyPress(
        SAConstants.KeyCode.LEFT_ARROW, {});
  }

  /**
   * Moves the text caret one word backwards (does nothing
   * if already at the beginning of the field). If the
   * text caret is in the middle of a word, moves the caret
   * to the beginning of that word.
   * @public
   */
  moveBackwardOneWord() {
    this.navigationManager_.simulateKeyPress(
        SAConstants.KeyCode.LEFT_ARROW, {ctrl: true});
  }

  /**
   * Moves the text caret one character forward (does nothing
   * if there are no more characters following the current
   * location of the caret).
   * @public
   */
  moveForwardOneChar() {
    this.navigationManager_.simulateKeyPress(
        SAConstants.KeyCode.RIGHT_ARROW, {});
  }

  /**
   * Moves the text caret one word forward (does nothing if
   * already at the end of the field). If the text caret is
   * in the middle of a word, moves the caret to the end of
   * that word.
   * @public
   */
  moveForwardOneWord() {
    this.navigationManager_.simulateKeyPress(
        SAConstants.KeyCode.RIGHT_ARROW, {ctrl: true});
  }

  /**
   * Moves the text caret one line up (does nothing
   * if there are no lines above the current location of
   * the caret).
   * @public
   */
  moveUpOneLine() {
    this.navigationManager_.simulateKeyPress(SAConstants.KeyCode.UP_ARROW, {});
  }

  /**
   * Moves the text caret one line down (does nothing
   * if there are no lines below the current location of
   * the caret).
   * @public
   */
  moveDownOneLine() {
    this.navigationManager_.simulateKeyPress(
        SAConstants.KeyCode.DOWN_ARROW, {});
  }


  /**
   * TODO(rosalindag): Work on text selection functionality below.
   */

  /**
   * Sets the selection using the selectionStart and selectionEnd
   * as the offset input for setDocumentSelection and the parameter
   * textNode as the object input for setDocumentSelection.
   * @private
   */
  saveSelection_() {
    if (this.selectionStartIndex_ == NO_SELECT_INDEX ||
        this.selectionEndIndex_ == NO_SELECT_INDEX) {
      console.log(
          'Selection bounds are not set properly:', this.selectionStartIndex_,
          this.selectionEndIndex_);
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
   * Returns the selection end index.
   * @return {number}
   * @public
   */
  getSelEndIndex() {
    return this.selectionEndIndex_;
  }

  /**
   * Reset the selectionStartIndex to NO_SELECT_INDEX.
   */
  resetSelStartIndex() {
    this.selectionStartIndex_ = NO_SELECT_INDEX;
  }

  /**
   * Returns the selection start index.
   * @return {number}
   * @public
   */
  getSelStartIndex() {
    return this.selectionStartIndex_;
  }

  /**
   * Sets the selection start index.
   * @param {number} startIndex
   * @param {!chrome.automation.AutomationNode} textNode
   * @public
   */
  setSelStartIndexAndNode(startIndex, textNode) {
    this.selectionStartIndex_ = startIndex;
    this.selectionStartObject_ = textNode;
  }

  /**
   * Returns if the selection start index is set in the current node.
   * @return {boolean}
   * @public
   */
  isSelectionStarted() {
    return this.selectionStartIndex_ !== NO_SELECT_INDEX;
  }

  /**
   * Returns either the selection start index or the selection end index of the
   * node based on the getStart param.
   * @param {!chrome.automation.AutomationNode} node
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
  saveSelectStart() {
    chrome.automation.getFocus((focusedNode) => {
      this.selectionStartObject_ = focusedNode;
      this.selectionStartIndex_ = this.getSelectionIndexFromNode_(
          this.selectionStartObject_, true /*We are getting the start index.*/);
    });
  }

  /**
   * Sets the selectionEnd variable based on the selection of the current node.
   * @public
   */
  saveSelectEnd() {
    chrome.automation.getFocus((focusedNode) => {
      this.selectionEndObject_ = focusedNode;
      this.selectionEndIndex_ = this.getSelectionIndexFromNode_(
          this.selectionEndObject_,
          false /*We are not getting the start index.*/);
      this.saveSelection_();
    });
  }
}
