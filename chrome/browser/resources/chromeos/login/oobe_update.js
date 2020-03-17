// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

Polymer({
  is: 'oobe-update-md',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Shows "Checking for update ..." section and hides "Updating..." section.
     */
    checkingForUpdate: {
      type: Boolean,
      value: true,
    },

    /**
     * Shows a warning to the user the update is about to proceed over a
     * cellular network, and asks the user to confirm.
     */
    requiresPermissionForCellular: {
      type: Boolean,
      value: false,
    },

    /**
     * Progress bar percent.
     */
    progressValue: {
      type: Number,
      value: 0,
    },

    /**
     * Estimated time left in seconds.
     */
    estimatedTimeLeft: {
      type: Number,
      value: 0,
    },

    /**
     * Shows estimatedTimeLeft.
     */
    estimatedTimeLeftShown: {
      type: Boolean,
    },

    /**
     * Message "33 percent done".
     */
    progressMessage: {
      type: String,
    },

    /**
     * True if update is fully completed and, probably manual action is
     * required.
     */
    updateCompleted: {
      type: Boolean,
      value: false,
    },

    /**
     * If update cancellation is allowed.
     */
    cancelAllowed: {
      type: Boolean,
      value: false,
    },

    /**
     * ID of the localized string for update cancellation message.
     */
    cancelHint: {
      type: String,
      value: 'cancelUpdateHint',
    },
  },

  onBeforeShow() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
    this.$['checking-downloading-update'].onBeforeShow();
  },

  onBackClicked_() {
    chrome.send('login.UpdateScreen.userActed', ['update-reject-cellular']);
  },

  onNextClicked_() {
    chrome.send('login.UpdateScreen.userActed', ['update-accept-cellular']);
  },
});
