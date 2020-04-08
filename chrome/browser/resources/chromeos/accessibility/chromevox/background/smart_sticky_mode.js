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

    // Several cases arise which may lead to a sticky mode toggle:
    // The node is either editable itself or a descendant of an editable.
    // The node is a relation target of an editable.
    const node = newRange.start.node;
    let shouldTurnOffStickyMode = false;
    if (node.state[chrome.automation.StateType.EDITABLE] ||
        (node.parent &&
         node.parent.state[chrome.automation.StateType.EDITABLE])) {
      // This covers both editable nodes, and inline text boxes (which are not
      // editable themselves, but may have an editable parent).
      shouldTurnOffStickyMode = true;
    } else {
      let focus = node;
      while (!shouldTurnOffStickyMode && focus) {
        if (focus.activeDescendantFor && focus.activeDescendantFor.length) {
          shouldTurnOffStickyMode |= focus.activeDescendantFor.some(
              (n) => n.state[chrome.automation.StateType.EDITABLE]);
        }

        if (focus.controlledBy && focus.controlledBy.length) {
          shouldTurnOffStickyMode |= focus.controlledBy.some(
              (n) => n.state[chrome.automation.StateType.EDITABLE]);
        }

        focus = focus.parent;
      }
    }

    // This toggler should not make any changes when the range isn't what we're
    // lloking for and we haven't previously tracked any sticky mode state from
    // the user.
    if (!shouldTurnOffStickyMode && !this.didTurnOffStickyMode_) {
      return;
    }

    if (shouldTurnOffStickyMode) {
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
