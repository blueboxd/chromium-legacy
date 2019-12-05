// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from ChromeVox's current range.
 */

goog.provide('RangeAutomationHandler');

goog.require('BaseAutomationHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

/**
 * @constructor
 * @implements {ChromeVoxStateObserver}
 * @extends {BaseAutomationHandler}
 */
RangeAutomationHandler = function() {
  BaseAutomationHandler.call(this, undefined);

  /** @private {AutomationNode} */
  this.lastAttributeTarget_;

  /** @private {Output} */
  this.lastAttributeOutput_;

  ChromeVoxState.addObserver(this);
};

RangeAutomationHandler.prototype = {
  __proto__: BaseAutomationHandler.prototype,

  /**
   * @param {cursors.Range} newRange
   */
  onCurrentRangeChanged: function(newRange) {
    if (this.node_) {
      this.removeAllListeners();
      this.node_ = undefined;
    }

    if (!newRange || !newRange.start.node || !newRange.end.node) {
      return;
    }

    this.node_ = AutomationUtil.getLeastCommonAncestor(
                     newRange.start.node, newRange.end.node) ||
        newRange.start.node;

    this.addListener_(
        EventType.ACTIVEDESCENDANTCHANGED, this.onActiveDescendantChanged);
    this.addListener_(
        EventType.ARIA_ATTRIBUTE_CHANGED, this.onAriaAttributeChanged);
    this.addListener_(EventType.AUTOCORRECTION_OCCURED, this.onEventIfInRange);
    this.addListener_(
        EventType.CHECKED_STATE_CHANGED, this.onCheckedStateChanged);
    this.addListener_(EventType.EXPANDED_CHANGED, this.onEventIfInRange);
    this.addListener_(EventType.INVALID_STATUS_CHANGED, this.onEventIfInRange);
    this.addListener_(EventType.LOCATION_CHANGED, this.onLocationChanged);
    this.addListener_(EventType.ROW_COLLAPSED, this.onEventIfInRange);
    this.addListener_(EventType.ROW_EXPANDED, this.onEventIfInRange);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onEventIfInRange: function(evt) {
    if (!DesktopAutomationHandler.announceActions &&
        evt.eventFrom == 'action') {
      return;
    }

    var prev = ChromeVoxState.instance.currentRange;
    if (!prev) {
      return;
    }

    // TODO: we need more fine grained filters for attribute changes.
    if (prev.contentEquals(cursors.Range.fromNode(evt.target)) ||
        evt.target.state.focused) {
      var prevTarget = this.lastAttributeTarget_;

      // Re-target to active descendant if it exists.
      var prevOutput = this.lastAttributeOutput_;
      this.lastAttributeTarget_ = evt.target.activeDescendant || evt.target;
      this.lastAttributeOutput_ = new Output().withRichSpeechAndBraille(
          cursors.Range.fromNode(this.lastAttributeTarget_), prev,
          Output.EventType.NAVIGATE);
      if (this.lastAttributeTarget_ == prevTarget && prevOutput &&
          prevOutput.equals(this.lastAttributeOutput_)) {
        return;
      }

      // If the target or an ancestor is controlled by another control, we may
      // want to delay the output.
      var maybeControlledBy = evt.target;
      while (maybeControlledBy) {
        if (maybeControlledBy.controlledBy.length &&
            maybeControlledBy.controlledBy.find((n) => !!n.autoComplete)) {
          clearTimeout(this.delayedAttributeOutputId_);
          this.delayedAttributeOutputId_ = setTimeout(() => {
            this.lastAttributeOutput_.go();
          }, DesktopAutomationHandler.ATTRIBUTE_DELAY_MS);
          return;
        }
        maybeControlledBy = maybeControlledBy.parent;
      }

      this.lastAttributeOutput_.go();
    }
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onAriaAttributeChanged: function(evt) {
    // Don't report changes on editable nodes since they interfere with text
    // selection changes. Users can query via Search+k for the current state of
    // the text field (which would also report the entire value).
    if (evt.target.state[StateType.EDITABLE]) {
      return;
    }

    // Only report attribute changes on some *Option roles if it is selected.
    if ((evt.target.role == RoleType.MENU_LIST_OPTION ||
         evt.target.role == RoleType.LIST_BOX_OPTION) &&
        !evt.target.selected)
      return;

    this.onEventIfInRange(evt);
  },

  /**
   * Handles active descendant changes.
   * @param {!AutomationEvent} evt
   */
  onActiveDescendantChanged: function(evt) {
    if (!evt.target.activeDescendant || !evt.target.state.focused) {
      return;
    }

    // Various events might come before a key press (which forces flushed
    // speech) and this handler. Force output to be at least category flushed.
    Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);
    this.onEventIfInRange(evt);
  },

  /**
   * Provides all feedback once a checked state changed event fires.
   * @param {!AutomationEvent} evt
   */
  onCheckedStateChanged: function(evt) {
    if (!AutomationPredicate.checkable(evt.target)) {
      return;
    }

    var event = new CustomAutomationEvent(
        EventType.CHECKED_STATE_CHANGED, evt.target, evt.eventFrom);
    this.onEventIfInRange(event);
  },

  /**
   * Updates the focus ring if the location of the current range, or
   * an descendant of the current range, changes.
   * @param {!AutomationEvent} evt
   */
  onLocationChanged: function(evt) {
    var cur = ChromeVoxState.instance.currentRange;
    if (!cur || !cur.isValid()) {
      if (ChromeVoxState.instance.getFocusBounds().length) {
        ChromeVoxState.instance.setFocusBounds([]);
      }
      return;
    }

    // Rather than trying to figure out if the current range falls somewhere in
    // |evt.target|, just update it if our cached bounds don't match.
    var oldFocusBounds = ChromeVoxState.instance.getFocusBounds();
    var startRect = cur.start.node.location;
    var endRect = cur.end.node.location;

    var found =
        oldFocusBounds.some((rect) => this.areRectsEqual_(rect, startRect)) &&
        oldFocusBounds.some((rect) => this.areRectsEqual_(rect, endRect));
    if (found) {
      return;
    }

    new Output().withLocation(cur, null, evt.type).go();
  },

  /**
   * @param {!chrome.accessibilityPrivate.ScreenRect} rectA
   * @param {!chrome.accessibilityPrivate.ScreenRect} rectB
   * @return {boolean} Whether the rects are the same.
   * @private
   */
  areRectsEqual_: function(rectA, rectB) {
    return rectA.left == rectB.left && rectA.top == rectB.top &&
        rectA.width == rectB.width && rectA.height == rectB.height;
  }
};
});  // goog.scope

new RangeAutomationHandler();
