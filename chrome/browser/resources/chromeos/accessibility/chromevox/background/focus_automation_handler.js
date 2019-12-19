// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation events on the currently focused node.
 */

goog.provide('FocusAutomationHandler');

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
 * @extends {BaseAutomationHandler}
 */
FocusAutomationHandler = function() {
  BaseAutomationHandler.call(this, null);

  /** @private {AutomationNode|undefined} */
  this.previousActiveDescendant_;

  chrome.automation.getDesktop((desktop) => {
    desktop.addEventListener(EventType.FOCUS, this.onFocus.bind(this), false);
  });
};

FocusAutomationHandler.prototype = {
  __proto__: BaseAutomationHandler.prototype,

  /**
   * @param {!AutomationEvent} evt
   */
  onFocus: function(evt) {
    this.removeAllListeners();
    this.previousActiveDescendant_ = evt.target.activeDescendant;
    this.node_ = evt.target;
    this.addListener_(
        EventType.ACTIVEDESCENDANTCHANGED, this.onActiveDescendantChanged);
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

    var prev = this.previousActiveDescendant_ ?
        cursors.Range.fromNode(this.previousActiveDescendant_) :
        ChromeVoxState.instance.currentRange;
    new Output()
        .withRichSpeechAndBraille(
            cursors.Range.fromNode(evt.target.activeDescendant), prev,
            Output.EventType.NAVIGATE)
        .go();
    this.previousActiveDescendant_ = evt.target.activeDescendant;
  }
};
});  // goog.scope

new FocusAutomationHandler();
