// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design reset
 * confirmation overlay screen.
 */

Polymer({
  is: 'reset-confirm-overlay-md',

  behaviors: [OobeI18nBehavior],

  properties: {
    isPowerwashView_: Boolean,
  },

  open() {
    if (!this.$.dialog.open)
      this.$.dialog.showModal();
  },

  close() {
    if (this.$.dialog.open)
      this.$.dialog.close();
  },

  /**
   * On-tap event handler for continue button.
   */
  onContinueTap_() {
    this.close();
    chrome.send('login.ResetScreen.userActed', ['powerwash-pressed']);
  },

  /**
   * On-tap event handler for cancel button.
   */
  onCancelTap_() {
    this.close();
    chrome.send('login.ResetScreen.userActed', ['reset-confirm-dismissed']);
  },
});
