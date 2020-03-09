// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

const CheckState = chrome.passwordsPrivate.PasswordCheckState;

Polymer({
  is: 'settings-password-check',

  behaviors: [I18nBehavior],

  properties: {
    /** @private */
    lastCompletedCheck_: String,

    /**
     * The number of compromised passwords as a formatted string.
     * @private
     */
    compromisedPasswordsCount_: String,

    /**
     * An array of leaked passwords to display.
     * @type {!Array<!PasswordManagerProxy.CompromisedCredential>}
     */
    leakedPasswords: {
      type: Array,
      // Initialized to the empty array on purpose because then array value is
      // undefined and hasLeakedCredentials_ isn't called from html on page
      // load.
      value: () => [],
    },

    /**
     * The status indicates progress and affects banner, title and icon.
     * @type {!PasswordManagerProxy.PasswordCheckStatus}
     * @private
     */
    status_: {
      type: PasswordManagerProxy.PasswordCheckStatus,
      value: () => {
        return {state: CheckState.IDLE};
      },
    }
  },

  /**
   * @type {?function(!PasswordManagerProxy.PasswordCheckStatus):void}
   * @private
   */
  statusChangedListener_: null,

  /**
   * @type {?function(!PasswordManagerProxy.CompromisedCredentialsInfo):void}
   * @private
   */
  leakedCredentialsListener_: null,

  /**
   * @private {PasswordManagerProxy}
   */
  passwordManager_: null,

  /**
   * The element to return focus to, when the currently active dialog is
   * closed.
   * @private {?HTMLElement}
   */
  activeDialogAnchor_: null,

  /** @override */
  attached() {
    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    const statusChangeListener = status => this.status_ = status;
    const setLeakedCredentialsListener = info => {
      this.leakedPasswords = info.compromisedCredentials;
      this.lastCompletedCheck_ = info.elapsedTimeSinceLastCheck;

      settings.PluralStringProxyImpl.getInstance()
          .getPluralString('compromisedPasswords', this.leakedPasswords.length)
          .then(count => {
            this.compromisedPasswordsCount_ = count;
          });
    };

    this.statusChangedListener_ = statusChangeListener;
    this.leakedCredentialsListener_ = setLeakedCredentialsListener;

    // Request initial data.
    this.passwordManager_.getPasswordCheckStatus().then(
        this.statusChangedListener_);
    this.passwordManager_.getCompromisedCredentialsInfo().then(
        this.leakedCredentialsListener_);

    // Listen for changes.
    this.passwordManager_.addPasswordCheckStatusListener(
        this.statusChangedListener_);
    this.passwordManager_.addCompromisedCredentialsListener(
        this.leakedCredentialsListener_);
  },

  /** @override */
  detached() {
    this.passwordManager_.removePasswordCheckStatusListener(
        assert(this.statusChangedListener_));
    this.statusChangedListener_ = null;
    this.passwordManager_.removeCompromisedCredentialsListener(
        assert(this.leakedCredentialsListener_));
    this.leakedCredentialsListener_ = null;
  },

  /**
   * Start/Stop bulk password check.
   * @private
   */
  onPasswordCheckButtonClick_() {
    // TODO(https://crbug.com/1047726): By click on this button user should be
    // able to 'Cancel' current check.
    this.passwordManager_.startBulkPasswordCheck();
  },

  /**
   * Returns true if there are any compromised credentials.
   * @param {!Array<!PasswordManagerProxy.CompromisedCredential>} list
   * @return {boolean}
   * @private
   */
  hasLeakedCredentials_(list) {
    return !!list.length;
  },

  /**
   * @param {!CustomEvent<{moreActionsButton: !HTMLElement}>} event
   * @private
   */
  onMoreActionsClick_(event) {
    const target = event.detail.moreActionsButton;
    this.$.moreActionsMenu.showAt(target);
    this.activeDialogAnchor_ = target;
  },

  /** @private */
  onMenuShowPasswordClick_() {
    this.$.moreActionsMenu.close();

    // TODO(crbug.com/1047726) Implement dialog.
  },

  /** @private */
  onMenuEditPasswordClick_() {
    this.$.moreActionsMenu.close();

    // TODO(crbug.com/1047726) Implement dialog.
  },

  /** @private */
  onMenuRemovePasswordClick_() {
    this.$.moreActionsMenu.close();

    // TODO(crbug.com/1047726) Implement dialog.
  },
});
})();
