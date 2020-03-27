// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automatic sticky mode toggles. Turns sticky mode off
 * when the current range is over an editable; restores sticky mode when not on
 * an editable.
 */

goog.provide('SmartStickyMode');

goog.require('ChromeVoxState');

/** @implements {ChromeVoxStateObserver} */
SmartStickyMode = class {
  constructor() {
    /** @private {boolean} */
    this.ignoreRangeChanges_ = false;

    /**
     * Tracks whether we (and not the user) turned off sticky mode when over an
     * editable.
     * @private {boolean}
     */
    this.didTurnOffStickyMode_ = false;

    ChromeVoxState.addObserver(this);
  }

  /** @override */
  onCurrentRangeChanged(newRange) {
    if (!newRange || this.ignoreRangeChanges_ ||
        localStorage['smartStickyMode'] !== 'true') {
      return;
    }

    const isRangeEditable =
        newRange.start.node.state[chrome.automation.StateType.EDITABLE];

    // This toggler should not make any changes when the range isn't editable
    // and we haven't previously tracked any sticky mode state from the user.
    if (!isRangeEditable && !this.didTurnOffStickyMode_) {
      return;
    }

    if (isRangeEditable) {
      if (!ChromeVox.isStickyPrefOn) {
        // Sticky mode was already off; do not track the current sticky state
        // since we may have set it ourselves.
        return;
      }

      if (this.didTurnOffStickyMode_) {
        // This should not be possible with |ChromeVox.isStickyPrefOn| set to
        // true.
        throw 'Unexpected sticky state value encountered.';
      }

      // Save the sticky state for restoration later.
      this.didTurnOffStickyMode_ = true;
      ChromeVoxBackground.setPref(
          'sticky', false /* value */, true /* announce */);
    } else if (this.didTurnOffStickyMode_) {
      // Restore the previous sticky mode state.
      ChromeVoxBackground.setPref(
          'sticky', true /* value */, true /* announce */);
      this.didTurnOffStickyMode_ = false;
    }
  }

  /**
   * When called, ignores all changes in the current range when toggling sticky
   * mode without user input.
   */
  startIgnoringRangeChanges() {
    this.ignoreRangeChanges_ = true;
  }

  /**
   * When called, stops ignoring changes in the current range when toggling
   * sticky mode without user input.
   */
  stopIgnoringRangeChanges() {
    this.ignoreRangeChanges_ = false;
  }
};
