// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview "Update is required to sign in" screen.
 */

login.createScreen('UpdateRequiredScreen', 'update-required', function() {
  /* Possible UI states of the error screen. */
  /** @const */ var UI_STATE = {
    UPDATE_REQUIRED_MESSAGE: 'update-required-message',
    UPDATE_PROCESS: 'update-process',
    UPDATE_NEED_PERMISSION: 'update-need-permission',
    UPDATE_COMPLETED_NEED_REBOOT: 'update-completed-need-reboot',
    UPDATE_ERROR: 'update-error',
    EOL_REACHED: 'eol',
    UPDATE_NO_NETWORK: 'update-no-network'
  };

  // Array of the possible UI states of the screen. Must be in the
  // same order as UpdateRequiredView::UIState enum values.
  /** @const */ var UI_STATES = [
    UI_STATE.UPDATE_REQUIRED_MESSAGE, UI_STATE.UPDATE_PROCESS,
    UI_STATE.UPDATE_NEED_PERMISSION, UI_STATE.UPDATE_COMPLETED_NEED_REBOOT,
    UI_STATE.UPDATE_ERROR, UI_STATE.EOL_REACHED, UI_STATE.UPDATE_NO_NETWORK
  ];

  return {
    EXTERNAL_API: [
      'setIsConnected', 'setUpdateProgressUnavailable',
      'setUpdateProgressValue', 'setUpdateProgressMessage',
      'setEstimatedTimeLeftVisible', 'setEstimatedTimeLeft', 'setUIState',
      'setEnterpriseAndDeviceName'
    ],

    /** Initial UI State for screen */
    getOobeUIInitialState() {
      return OOBE_UI_STATE.BLOCKING;
    },

    /**
     * Returns default event target element.
     * @type {Object}
     */
    get defaultControl() {
      return $('update-required-card');
    },

    /** @param {string} domain Enterprise domain name */
    /** @param {string} device Device name */
    setEnterpriseAndDeviceName(enterpriseDomain, device) {
      $('update-required-card').enterpriseDomain = enterpriseDomain;
      $('update-required-card').deviceName = device;
    },

    /** @param {boolean} connected */
    setIsConnected(connected) {
      $('update-required-card').isNetworkConnected = connected;
    },

    /**
     * @param {boolean} unavailable.
     */
    setUpdateProgressUnavailable(unavailable) {
      $('update-required-card').updateProgressUnavailable = unavailable;
    },

    /**
     * Sets update's progress bar value.
     * @param {number} progress Percentage of the progress bar.
     */
    setUpdateProgressValue(progress) {
      $('update-required-card').updateProgressValue = progress;
    },

    /**
     * Sets message below progress bar.
     * @param {string} message Message that should be shown.
     */
    setUpdateProgressMessage(message) {
      $('update-required-card').updateProgressMessage = message;
    },

    /**
     * Shows or hides downloading ETA message.
     * @param {boolean} visible Are ETA message visible?
     */
    setEstimatedTimeLeftVisible(visible) {
      $('update-required-card').estimatedTimeLeftVisible = visible;
    },

    /**
     * Sets estimated time left until download will complete.
     * @param {number} seconds Time left in seconds.
     */
    setEstimatedTimeLeft(seconds) {
      $('update-required-card').estimatedTimeLeft = seconds;
    },

    /**
     * Sets current UI state of the screen.
     * @param {number} ui_state New UI state of the screen.
     */
    setUIState(ui_state) {
      $('update-required-card').ui_state = UI_STATES[ui_state];
    },
  };
});
