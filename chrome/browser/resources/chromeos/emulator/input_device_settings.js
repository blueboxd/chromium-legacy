// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'input-device-settings',

  behaviors: [WebUIListenerBehavior],

  ready: function() {
    this.addWebUIListener(
        'touchpad-exists-changed', this.setTouchpadExists_.bind(this));
    this.addWebUIListener(
        'mouse-exists-changed', this.setMouseExists_.bind(this));
  },

  /**
   * @param {!Event} e
   * Callback when the user toggles the touchpad.
   */
  onTouchpadChange: function(e) {
    chrome.send('setHasTouchpad', [e.target.checked]);
    this.$.changeDescription.opened = true;
  },

  /**
   * @param {!Event} e
   * Callback when the user toggles the mouse.
   */
  onMouseChange: function(e) {
    chrome.send('setHasMouse', [e.target.checked]);
    this.$.changeDescription.opened = true;
  },

  /**
   * Callback when the existence of a fake mouse changes.
   * @param {boolean} exists
   * @private
   */
  setMouseExists_: function(exists) {
    this.$.mouse.checked = exists;
  },

  /**
   * Callback when the existence of a fake touchpad changes.
   * @param {boolean} exists
   * @private
   */
  setTouchpadExists_: function(exists) {
    this.$.touchpad.checked = exists;
  },
});
