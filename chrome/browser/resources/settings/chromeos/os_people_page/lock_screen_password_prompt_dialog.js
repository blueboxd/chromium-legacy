// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-lock-screen-password-prompt-dialog' displays a password prompt to
 * the user. Clients can determine if the user has authenticated if either the
 * |authToken| string or |setModes| closure are present.
 *
 * The setModes binding is a wrapper around chrome.quickUnlockPrivate.setModes
 * which has a prebound account password. The account password by itself is not
 * available for other elements to access.
 *
 * Example:
 *
 * <settings-lock-screen-password-prompt-dialog
 *   id="lockScreenPasswordPrompt"
 *   set-modes="[[setModes]]">
 * </settings-lock-screen-password-prompt-dialog>
 */

(function() {
'use strict';

Polymer({
  is: 'settings-lock-screen-password-prompt-dialog',

  behaviors: [
    LockStateBehavior,
  ],

  properties: {
    /**
     * Authentication token returned by quickUnlockPrivate.getAuthToken. Should
     * be passed to API calls which require authentication.
     * @type {string}
     */
    authToken: {
      type: String,
      notify: true,
      observer: 'authTokenChanged_',
    },

    /**
     * A wrapper around chrome.quickUnlockPrivate.setModes with the account
     * password already supplied. If this is null, the authentication screen
     * needs to be redisplayed. This property will be cleared after the timeout
     * returned by quickUnlockPrivate.getAuthToken.
     * @type {?Function}
     */
    setModes: {
      type: Object,
      notify: true,
    },

    /**
     * writeUma_ is a function that handles writing uma stats. It may be
     * overridden for tests.
     *
     * @type {Function}
     * @private
     */
    writeUma_: {
      type: Object,
      value: function() {
        return settings.recordLockScreenProgress;
      }
    },
  },

  /** @override */
  attached: function() {
    this.writeUma_(LockScreenProgress.START_SCREEN_LOCK);
  },

  /**
   * Called when the authToken changes. If the authToken is valid, that
   * indicates the user authenticated successfully.
   * @param {String} authToken
   * @private
   */
  authTokenChanged_: function(authToken) {
    if (!this.authToken) {
      this.setModes = null;
      return;
    }

    // The user successfully authenticated.
    this.writeUma_(LockScreenProgress.ENTER_PASSWORD_CORRECTLY);

    this.setModes = (modes, credentials, onComplete) => {
      this.quickUnlockPrivate.setModes(
          this.authToken, modes, credentials, () => {
            let result = true;
            if (chrome.runtime.lastError) {
              console.error(
                  'setModes failed: ' + chrome.runtime.lastError.message);
              result = false;
            }
            onComplete(result);
          });
    };
  },

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @return {string}
   * @private
   */
  selectPasswordPromptEnterPasswordString_: function(hasPinLogin) {
    if (hasPinLogin) {
      return this.i18n('passwordPromptEnterPasswordLoginLock');
    }
    return this.i18n('passwordPromptEnterPasswordLock');
  },
});
})();
