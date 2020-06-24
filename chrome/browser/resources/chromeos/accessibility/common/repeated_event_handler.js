// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class assists with processing repeated events in nontrivial ways by
 * allowing only the most recent event to be processed.
 */
class RepeatedEventHandler {
  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.EventType} type
   * @param {!function(!chrome.automation.AutomationEvent)} callback
   * @param {boolean} capture
   */
  constructor(node, type, callback, capture = false) {
    /** @private {!Array<!chrome.automation.AutomationEvent>} */
    this.eventStack_ = [];

    /** @private {!chrome.automation.AutomationNode} */
    this.node_ = node;

    /** @private {!chrome.automation.EventType} */
    this.type_ = type;

    /** @private {!function(!chrome.automation.AutomationEvent)} */
    this.callback_ = callback;

    /** @private {boolean} */
    this.capture_ = capture;

    /** @private {!function(!chrome.automation.AutomationEvent)} */
    this.handler_ = this.onEvent_.bind(this);

    node.addEventListener(type, this.handler_, capture);
  }

  /** Stops listening or handling future events. */
  stopListening() {
    this.node_.removeEventListener(this.type_, this.handler_, this.capture_);
  }

  /**
   * @param {!chrome.automation.AutomationEvent} event
   * @private
   */
  onEvent_(event) {
    this.eventStack_.push(event);
    setTimeout(this.handleEvent_.bind(this), 0);
  }

  /** @private */
  handleEvent_() {
    if (this.eventStack_.length === 0) {
      return;
    }
    const event = this.eventStack_.pop();
    this.eventStack_ = [];

    this.callback_(event);
  }
}
