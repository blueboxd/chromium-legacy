// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for the security token PIN dialog shown during
 * sign-in.
 */

Polymer({
  is: 'security-token-pin',

  behaviors: [OobeDialogHostBehavior],

  properties: {
    /**
     * Contains the OobeTypes.SecurityTokenPinDialogParameters object. It can be
     * null when our element isn't used.
     */
    parameters: {
      type: Object,
    },

    /**
     * Whether the current state is the wait for the processing completion
     * (i.e., the backend is verifying the entered PIN).
     * @private
     */
    processingCompletion_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Resets the dialog to the initial state.
   */
  reset: function() {
    this.$.pinKeyboard.value = '';
    this.processingCompletion_ = false;
  },

  /**
   * Invoked when the "Back" button is clicked.
   * @private
   */
  onBackClicked_: function() {
    this.fire('cancel');
  },

  /**
   * Invoked when the "Next" button is clicked.
   * @private
   */
  onNextClicked_: function() {
    this.processingCompletion_ = true;
    this.fire('completed', this.$.pinKeyboard.value);
  },
});
