// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Demo Setup
 * screen.
 */

Polymer({
  is: 'demo-setup-md',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /** Object mapping step strings to step indices */
    setupSteps_: {
      type: Object,
      value() {
        return /** @type {!Object} */ (loadTimeData.getValue('demoSetupSteps'));
      }
    },

    /** Which step index is currently running in Demo Mode setup. */
    currentStepIndex_: {
      type: Number,
      value: -1,
    },

    /** Error message displayed on demoSetupErrorDialog screen. */
    errorMessage_: {
      type: String,
      value: '',
    },

    /** Whether powerwash is required in case of a setup error. */
    isPowerwashRequired_: {
      type: Boolean,
      value: false,
    },

    /** Ordered array of screen ids that are a part of demo setup flow. */
    screens_: {
      type: Array,
      readonly: true,
      value() {
        return ['demoSetupProgressDialog', 'demoSetupErrorDialog'];
      },
    },

    /** Feature flag to display progress bar instead of spinner during setup. */
    showStepsInDemoModeSetup_: {
      type: Boolean,
      readonly: true,
      value() {
        return loadTimeData.getBoolean('showStepsInDemoModeSetup');
      }
    }
  },

  /** Resets demo setup flow to the initial screen and starts setup. */
  reset() {
    this.showScreen_('demoSetupProgressDialog');
    chrome.send('login.DemoSetupScreen.userActed', ['start-setup']);
  },

  /** Called after resources are updated. */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * Called at the beginning of a setup step.
   * @param {string} currentStep The new step name.
   */
  setCurrentSetupStep(currentStep) {
    // If new step index not specified, remain unchanged.
    if (this.setupSteps_.hasOwnProperty(currentStep)) {
      this.currentStepIndex_ = this.setupSteps_[currentStep];
    }
  },

  /** Called when demo mode setup succeeded. */
  onSetupSucceeded() {
    this.errorMessage_ = '';
  },

  /**
   * Called when demo mode setup failed.
   * @param {string} message Error message to be displayed to the user.
   * @param {boolean} isPowerwashRequired Whether powerwash is required to
   *     recover from the error.
   */
  onSetupFailed(message, isPowerwashRequired) {
    this.errorMessage_ = message;
    this.isPowerwashRequired_ = isPowerwashRequired;
    this.showScreen_('demoSetupErrorDialog');
  },

  /**
   * Shows screen with the given id. Method exposed for testing environment.
   * @param {string} id Screen id.
   */
  showScreenForTesting(id) {
    this.showScreen_(id);
  },

  /**
   * Shows screen with the given id.
   * @param {string} id Screen id.
   * @private
   */
  showScreen_(id) {
    this.hideScreens_();

    var screen = this.$[id];
    assert(screen);
    screen.hidden = false;
    screen.show();
  },

  /**
   * Hides all screens to help switching from one screen to another.
   * @private
   */
  hideScreens_() {
    for (let id of this.screens_) {
      var screen = this.$[id];
      assert(screen);
      screen.hidden = true;
    }
  },

  /**
   * Retry button click handler.
   * @private
   */
  onRetryClicked_() {
    this.reset();
  },

  /**
   * Powerwash button click handler.
   * @private
   */
  onPowerwashClicked_() {
    chrome.send('login.DemoSetupScreen.userActed', ['powerwash']);
  },

  /**
   * Close button click handler.
   * @private
   */
  onCloseClicked_() {
    // TODO(wzang): Remove this after crbug.com/900640 is fixed.
    if (this.isPowerwashRequired_)
      return;
    chrome.send('login.DemoSetupScreen.userActed', ['close-setup']);
  },

  /**
   * Whether a given step should be rendered on the UI.
   * @param {string} stepName The name of the step (from the enum).
   * @param {!Object} setupSteps
   * @private
   */
  shouldShowStep_(stepName, setupSteps) {
    return setupSteps.hasOwnProperty(stepName);
  },

  /**
   * Whether a given step is active.
   * @param {string} stepName The name of the step (from the enum).
   * @param {!Object} setupSteps
   * @param {number} currentStepIndex
   * @private
   */
  stepIsActive_(stepName, setupSteps, currentStepIndex) {
    return currentStepIndex == setupSteps[stepName];
  },

  /**
   * Whether a given step is completed.
   * @param {string} stepName The name of the step (from the enum).
   * @param {!Object} setupSteps
   * @param {number} currentStepIndex
   * @private
   */
  stepIsCompleted_(stepName, setupSteps, currentStepIndex) {
    return currentStepIndex > setupSteps[stepName];
  },
});
