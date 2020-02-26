// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Device reset screen implementation.
 */

login.createScreen('ResetScreen', 'reset', function() {
  var USER_ACTION_CANCEL_RESET = 'cancel-reset';
  var USER_ACTION_RESET_CONFIRM_DISMISSED = 'reset-confirm-dismissed';

  /* Possible UI states of the reset screen. */
  const RESET_SCREEN_UI_STATE = {
    REVERT_PROMISE: 'ui-state-revert-promise',
    RESTART_REQUIRED: 'ui-state-restart-required',
    POWERWASH_PROPOSAL: 'ui-state-powerwash-proposal',
    ROLLBACK_PROPOSAL: 'ui-state-rollback-proposal',
    ERROR: 'ui-state-error',
  };

  const RESET_SCREEN_STATE = {
    RESTART_REQUIRED: 0,
    REVERT_PROMISE: 1,
    POWERWASH_PROPOSAL: 2,  // supports 2 ui-states
    ERROR: 3,
  };

  return {
    EXTERNAL_API: [
      'setIsRollbackAvailable',
      'setIsRollbackChecked',
      'setIsTpmFirmwareUpdateAvailable',
      'setIsTpmFirmwareUpdateChecked',
      'setIsTpmFirmwareUpdateEditable',
      'setTpmFirmwareUpdateMode',
      'setIsConfirmational',
      'setIsGoogleBrandedBuild',
      'setScreenState',
    ],

    /** @type {boolean} */
    isRollbackAvailable_: false,
    /** @type {boolean} */
    isRollbackChecked_: false,
    /** @type {boolean} */
    isTpmFirmwareUpdateAvailable_: false,
    /** @type {boolean} */
    isTpmFirmwareUpdateChecked_: false,
    /** @type {boolean} */
    isTpmFirmwareUpdateEditable_: false,
    /** @type {RESET_SCREEN_UI_STATE} */
    tpmFirmwareUpdateMode_: RESET_SCREEN_UI_STATE.REVERT_PROMISE,
    /** @type {boolean} */
    isConfirmational_: false,
    /** @type {RESET_SCREEN_STATE} */
    screenState_: RESET_SCREEN_STATE.RESTART_REQUIRED,

    setIsRollbackAvailable(rollbackAvailable) {
      this.isRollbackAvailable_ = rollbackAvailable;
      this.setRollbackOptionView();
    },

    setIsRollbackChecked(rollbackChecked) {
      this.isRollbackChecked_ = rollbackChecked;
      this.setRollbackOptionView();
    },

    setIsTpmFirmwareUpdateAvailable(value) {
      this.isTpmFirmwareUpdateAvailable_ = value;
      this.setTPMFirmwareUpdateView_();
    },

    setIsTpmFirmwareUpdateChecked(value) {
      this.isTpmFirmwareUpdateChecked_ = value;
      this.setTPMFirmwareUpdateView_();
    },

    setIsTpmFirmwareUpdateEditable(value) {
      this.isTpmFirmwareUpdateEditable_ = value;
      this.setTPMFirmwareUpdateView_();
    },

    setTpmFirmwareUpdateMode(value) {
      this.tpmFirmwareUpdateMode_ = value;
    },

    setIsConfirmational(isConfirmational) {
      this.isConfirmational_ = isConfirmational;
      if (isConfirmational) {
        if (this.screenState_ != RESET_SCREEN_STATE.POWERWASH_PROPOSAL)
          return;
        $('overlay-reset').removeAttribute('hidden');
        $('reset-confirm-overlay-md').open();
      } else {
        $('overlay-reset').setAttribute('hidden', true);
        $('reset-confirm-overlay-md').close();
      }
    },

    setIsGoogleBrandedBuild(isGoogleBranded) {
      $('oobe-reset-md').isGoogleBranded_ = isGoogleBranded;
    },

    setScreenState(state) {
      this.screenState_ = state;

      if (state == RESET_SCREEN_STATE.RESTART_REQUIRED)
        this.ui_state = RESET_SCREEN_UI_STATE.RESTART_REQUIRED;
      if (state == RESET_SCREEN_STATE.REVERT_PROMISE)
        this.ui_state = RESET_SCREEN_UI_STATE.REVERT_PROMISE;
      else if (state == RESET_SCREEN_STATE.POWERWASH_PROPOSAL)
        this.ui_state = RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL;
      this.setDialogView_();
      if (state == RESET_SCREEN_STATE.REVERT_PROMISE) {
        announceAccessibleMessage(
            loadTimeData.getString('resetRevertSpinnerMessage'));
      }
      this.setTPMFirmwareUpdateView_();
    },

    /** @override */
    decorate() {
      $('oobe-reset-md').screen = this;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('oobe-reset-md');
    },

    /**
     * Cancels the reset and drops the user back to the login screen.
     */
    cancel() {
      if (this.isConfirmational_) {
        $('reset').send(
            login.Screen.CALLBACK_USER_ACTED,
            USER_ACTION_RESET_CONFIRM_DISMISSED);
        return;
      }
      this.send(login.Screen.CALLBACK_USER_ACTED, USER_ACTION_CANCEL_RESET);
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow(data) {},

    /** Event handler that is invoked after the screen is shown. */
    onAfterShow() {
      Oobe.resetSigninUI(false);
    },

    /**
     * Sets css style for corresponding state of the screen.
     * @private
     */
    setDialogView_(state) {
      state = this.ui_state;
      this.classList.toggle(
          'revert-promise-view', state == RESET_SCREEN_UI_STATE.REVERT_PROMISE);
      this.classList.toggle(
          'restart-required-view',
          state == RESET_SCREEN_UI_STATE.RESTART_REQUIRED);
      this.classList.toggle(
          'powerwash-proposal-view',
          state == RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL);
      this.classList.toggle(
          'rollback-proposal-view',
          state == RESET_SCREEN_UI_STATE.ROLLBACK_PROPOSAL);
      var resetMd = $('oobe-reset-md');
      var resetOverlayMd = $('reset-confirm-overlay-md');
      if (state == RESET_SCREEN_UI_STATE.RESTART_REQUIRED) {
        resetMd.uiState_ = 'restart-required-view';
      }
      if (state == RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL) {
        resetMd.uiState_ = 'powerwash-proposal-view';
        resetOverlayMd.isPowerwashView_ = true;
      }
      if (state == RESET_SCREEN_UI_STATE.ROLLBACK_PROPOSAL) {
        resetMd.uiState_ = 'rollback-proposal-view';
        resetOverlayMd.isPowerwashView_ = false;
      }
      if (state == RESET_SCREEN_UI_STATE.REVERT_PROMISE) {
        resetMd.uiState_ = 'revert-promise-view';
      }
    },

    setRollbackOptionView() {
      if (this.isConfirmational_)
        return;
      if (this.screenState_ != RESET_SCREEN_STATE.POWERWASH_PROPOSAL)
        return;

      if (this.isRollbackAvailable_ && this.isRollbackChecked_) {
        this.ui_state = RESET_SCREEN_UI_STATE.ROLLBACK_PROPOSAL;
      } else {
        this.ui_state = RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL;
      }
      this.setDialogView_();
      this.setTPMFirmwareUpdateView_();
    },

    setTPMFirmwareUpdateView_() {
      $('oobe-reset-md').tpmFirmwareUpdateAvailable_ =
          this.ui_state == RESET_SCREEN_UI_STATE.POWERWASH_PROPOSAL &&
          this.isTpmFirmwareUpdateAvailable_;
      $('oobe-reset-md').tpmFirmwareUpdateChecked_ =
          this.isTpmFirmwareUpdateChecked_;
      $('oobe-reset-md').tpmFirmwareUpdateEditable_ =
          this.isTpmFirmwareUpdateEditable_;
    },

    onTPMFirmwareUpdateChanged_(value) {
      chrome.send('ResetScreen.setTpmFirmwareUpdateChecked', [value]);
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent() {
      $('oobe-reset-md').i18nUpdateLocale();
      $('reset-confirm-overlay-md').i18nUpdateLocale();
    },
  };
});
