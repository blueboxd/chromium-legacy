// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for displaying material design Enable Kiosk
 * screen.
 */

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const EnableKioskMode = {
  CONFIRM: 'confirm',
  SUCCESS: 'success',
  ERROR: 'error',
};

Polymer({
  is: 'kiosk-enable',

  behaviors: [I18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {
    /**
     * Current dialog state
     * @type {EnableKioskMode}
     * @private
     */
    state_: {
      type: String,
      value: EnableKioskMode.CONFIRM,
    },
  },

  EXTERNAL_API: [
    'onCompleted',
  ],

  /** @override */
  ready: function() {
    this.initializeLoginScreen('KioskEnableScreen', {
      resetAllowed: true,
    });
  },

  /** Called after resources are updated. */
  updateLocalizedContent: function() {
    this.i18nUpdateLocale();
  },

  /** Called when dialog is shown */
  onBeforeShow: function() {
    this.state_ = EnableKioskMode.CONFIRM;
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
  },

  /**
   * "Enable" button handler
   * @private
   */
  onEnableButton_: function(event) {
    chrome.send('kioskOnEnable');
  },

  /**
   * "Cancel" / "Ok" button handler
   * @private
   */
  closeDialog_: function(event) {
    chrome.send('kioskOnClose');
  },

  onCompleted: function(success) {
    this.state_ = success ? EnableKioskMode.SUCCESS : EnableKioskMode.ERROR;
  },

  /**
   * Simple equality comparison function.
   * @private
   */
  eq_: function(one, another) {
    return one === another;
  },

  /**
   *
   * @private
   */
  primaryButtonText_(locale, state) {
    if (state === EnableKioskMode.CONFIRM)
      return this.i18n('kioskOKButton');
    return this.i18n('kioskCancelButton');
  }
});
