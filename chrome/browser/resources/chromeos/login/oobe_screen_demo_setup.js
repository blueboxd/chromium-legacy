// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo mode setup screen implementation.
 */

login.createScreen('DemoSetupScreen', 'demo-setup', function() {
  return {
    EXTERNAL_API: ['setCurrentSetupStep', 'onSetupSucceeded', 'onSetupFailed'],

    /**
     * Demo setup module.
     * @private
     */
    demoSetupModule_: null,


    /** @override */
    decorate() {
      this.demoSetupModule_ = $('demo-setup-content');
    },

    /** Returns a control which should receive an initial focus. */
    get defaultControl() {
      return this.demoSetupModule_;
    },

    /** Called after resources are updated. */
    updateLocalizedContent() {
      this.demoSetupModule_.updateLocalizedContent();
    },

    /** @override */
    onBeforeShow() {
      this.demoSetupModule_.reset();
    },

    /**
     * Called at the beginning of a setup step.
     * @param {number} currentStepIndex
     */
    setCurrentSetupStep(currentStepIndex) {
      this.demoSetupModule_.setCurrentSetupStep(currentStepIndex);
    },

    /** Called when demo mode setup succeeded. */
    onSetupSucceeded() {
      this.demoSetupModule_.onSetupSucceeded();
    },

    /**
     * Called when demo mode setup failed.
     * @param {string} message Error message to be displayed to the user.
     * @param {boolean} isPowerwashRequired Whether powerwash is required to
     *     recover from the error.
     */
    onSetupFailed(message, isPowerwashRequired) {
      this.demoSetupModule_.onSetupFailed(message, isPowerwashRequired);
    },
  };
});
