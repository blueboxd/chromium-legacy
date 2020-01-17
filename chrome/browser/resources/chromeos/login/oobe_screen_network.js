// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview OOBE network selection screen implementation.
 */

login.createScreen('NetworkScreen', 'network-selection', function() {
  return {
    EXTERNAL_API: ['showError', 'setOfflineDemoModeEnabled'],

    /** Dropdown element for networks selection. */
    dropdown_: null,

    /**
     * Network selection module.
     * @private
     */
    networkModule_: null,

    /** @override */
    decorate() {
      this.networkModule_ = $('oobe-network-md');
      this.networkModule_.screen = this;
      this.networkModule_.enabled = true;
    },

    /** Returns a control which should receive an initial focus. */
    get defaultControl() {
      return this.networkModule_;
    },

    /** @override */
    onBeforeShow(data) {
      var isDemoModeSetupKey = 'isDemoModeSetup';
      var isDemoModeSetup =
          data && isDemoModeSetupKey in data && data[isDemoModeSetupKey];
      this.networkModule_.isDemoModeSetup = isDemoModeSetup;
      this.networkModule_.show();
    },

    /**
     * Shows the network error message.
     * @param {string} message Message to be shown.
     */
    showError(message) {
      var error = document.createElement('div');
      var messageDiv = document.createElement('div');
      messageDiv.className = 'error-message-bubble';
      messageDiv.textContent = message;
      error.appendChild(messageDiv);
      error.setAttribute('role', 'alert');
    },

    /**
     * Enables or disables the offline Demo Mode option.
     * @param {bool} enabled
     */
    setOfflineDemoModeEnabled(enabled) {
      this.networkModule_.offlineDemoModeEnabled = enabled;
    },

    /** Called after resources are updated. */
    updateLocalizedContent() {
      this.networkModule_.updateLocalizedContent();
    },
  };
});
