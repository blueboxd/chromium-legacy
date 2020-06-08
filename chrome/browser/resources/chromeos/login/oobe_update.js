// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

'use strict';

(function() {

const USER_ACTION_ACCEPT_UPDATE_OVER_CELLUAR = 'update-accept-cellular';
const USER_ACTION_REJECT_UPDATE_OVER_CELLUAR = 'update-reject-cellular';
const USER_ACTION_CANCEL_UPDATE_SHORTCUT = 'cancel-update';

Polymer({
  is: 'oobe-update',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'setEstimatedTimeLeft',
    'showEstimatedTimeLeft',
    'setUpdateCompleted',
    'showUpdateCurtain',
    'setProgressMessage',
    'setUpdateProgress',
    'setRequiresPermissionForCellular',
    'setCancelUpdateShortcutEnabled',
  ],

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

  ready() {
    this.initializeLoginScreen('UpdateScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Cancels the screen.
   */
  cancel() {
    this.cancelHint = 'cancelledUpdateMessage';
    this.userActed(USER_ACTION_CANCEL_UPDATE_SHORTCUT);
  },

  onBeforeShow() {
    cr.ui.login.invokePolymerMethod(
        this.$['checking-downloading-update'], 'onBeforeShow');
  },

  onBackClicked_() {
    this.userActed(USER_ACTION_REJECT_UPDATE_OVER_CELLUAR);
  },

  onNextClicked_() {
    this.userActed(USER_ACTION_ACCEPT_UPDATE_OVER_CELLUAR);
  },

  /** @param {boolean} enabled */
  setCancelUpdateShortcutEnabled(enabled) {
    this.cancelAllowed = enabled;
  },

  /**
   * Sets update's progress bar value.
   * @param {number} progress Percentage of the progress bar.
   */
  setUpdateProgress(progress) {
    this.progressValue = progress;
  },

  /**
   * Shows or hides the warning that asks the user for permission to update
   * over celluar.
   * @param {boolean} requiresPermission Are the warning visible?
   */
  setRequiresPermissionForCellular(requiresPermission) {
    this.requiresPermissionForCellular = requiresPermission;
  },

  /**
   * Shows or hides downloading ETA message.
   * @param {boolean} visible Are ETA message visible?
   */
  showEstimatedTimeLeft(visible) {
    this.estimatedTimeLeftShown = visible;
  },

  /**
   * Sets estimated time left until download will complete.
   * @param {number} seconds Time left in seconds.
   */
  setEstimatedTimeLeft(seconds) {
    this.estimatedTimeLeft = seconds;
  },

  /**
   * Sets message below progress bar. Hide the message by setting an empty
   * string.
   * @param {string} message Message that should be shown.
   */
  setProgressMessage(message) {
    let visible = !!message;
    this.progressMessage = message;
    this.estimatedTimeLeftShown = !visible;
  },

  /**
   * Marks update completed. Shows "update completed" message.
   * @param {boolean} is_completed True if update process is completed.
   */
  setUpdateCompleted(is_completed) {
    this.updateCompleted = is_completed;
  },

  /**
   * Shows or hides update curtain.
   * @param {boolean} visible Are curtains visible?
   */
  showUpdateCurtain(visible) {
    this.checkingForUpdate = visible;
  },

});
})();
