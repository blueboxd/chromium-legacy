// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo mode setup screen implementation.
 */

login.createScreen('DemoSetupScreen', 'demo-setup', function() {
  return {
    EXTERNAL_API:
        ['incrementSetupProgress', 'onSetupSucceeded', 'onSetupFailed'],

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
     * Called when the progress bar needs updating with a new percentage value.
     * @param {number} percentage Number in range 0-100 denoting progress
     * percentage.
     */
    incrementSetupProgress(complete) {
      this.demoSetupModule_.incrementSetupProgress(complete);
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
