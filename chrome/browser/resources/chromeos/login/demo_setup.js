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
    /** Percentage of progress completed in Demo Mode setup. */
    progressPercentage_: {
      type: Number,
      value: 0,
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
    showProgressBarInDemoModeSetup_: {
      type: Boolean,
      readonly: true,
      value() {
        return loadTimeData.getBoolean('showProgressBarInDemoModeSetup');
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
   * Called when the progress bar needs to be incremented. Every time progress
   * is incremented, remaining progress is halved.
   * @param {boolean} complete Set to true if progress is complete
   */
  incrementSetupProgress(complete) {
    const maxPercentage = 100;
    if (complete) {
      this.progressPercentage_ = maxPercentage;
    } else {
      const remaining = maxPercentage - this.progressPercentage_;
      this.progressPercentage_ += remaining / 2;
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
});
