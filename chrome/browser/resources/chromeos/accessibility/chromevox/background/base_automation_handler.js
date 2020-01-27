// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Basic facillities to handle events from a single automation
 * node.
 */

goog.provide('BaseAutomationHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var EventType = chrome.automation.EventType;

BaseAutomationHandler = class {
  /**
   * @param {AutomationNode|undefined} node
   */
  constructor(node) {
    /**
     * @type {AutomationNode|undefined}
     */
    this.node_ = node;

    /**
     * @type {!Object<EventType,
     *     function(!AutomationEvent): void>} @private
     */
    this.listeners_ = {};
  }

  /**
   * Adds an event listener to this handler.
   * @param {EventType} eventType
   * @param {!function(!AutomationEvent): void} eventCallback
   * @protected
   */
  addListener_(eventType, eventCallback) {
    if (this.listeners_[eventType]) {
      throw 'Listener already added: ' + eventType;
    }

    var listener = this.makeListener_(eventCallback.bind(this));
    this.node_.addEventListener(eventType, listener, true);
    this.listeners_[eventType] = listener;
  }

  /**
   * Removes all listeners from this handler.
   */
  removeAllListeners() {
    for (var eventType in this.listeners_) {
      this.node_.removeEventListener(
          eventType, this.listeners_[eventType], true);
    }

    this.listeners_ = {};
  }

  /**
   * @return {!function(!AutomationEvent): void}
   * @private
   */
  makeListener_(callback) {
    return function(evt) {
      if (this.willHandleEvent_(evt)) {
        return;
      }
      callback(evt);
      this.didHandleEvent_(evt);
    }.bind(this);
  }

  /**
   * Called before the event |evt| is handled.
   * @return {boolean} True to skip processing this event.
   * @protected
   */
  willHandleEvent_(evt) {
    return false;
  }

  /**
   * Called after the event |evt| is handled.
   * @protected
   */
  didHandleEvent_(evt) {}
};

});  // goog.scope
