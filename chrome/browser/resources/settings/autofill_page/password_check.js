// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

const CheckState = chrome.passwordsPrivate.PasswordCheckState;

Polymer({
  is: 'settings-password-check',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * This URL redirects to the passwords check for sync users.
     * @type {!string}
     * @private
     */
    passwordCheckUrl_: {
      type: String,
      value:
          'https://passwords.google.com/checkup/start?utm_source=chrome&utm_medium=desktop&utm_campaign=leak_dialog&hideExplanation=true',
    },

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
   * @type {?function(!PasswordManagerProxy.CompromisedCredentials):void}
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
    const setLeakedCredentialsListener = compromisedCredentials => {
      this.leakedPasswords = compromisedCredentials;

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
    this.passwordManager_.getCompromisedCredentials().then(
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
    switch (this.status_.state) {
      case CheckState.RUNNING:
        this.passwordManager_.stopBulkPasswordCheck();
        return;
      case CheckState.IDLE:
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.NO_PASSWORDS:
      case CheckState.OTHER_ERROR:
        this.passwordManager_.startBulkPasswordCheck();
        return;
      case CheckState.TOO_MANY_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
    }
    throw 'Can\'t trigger an action for state: ' + this.status_.state;
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

  /**
   * Returns the icon (warning, info or error) indicating the check status.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @param {!Array<!PasswordManagerProxy.CompromisedCredential>}
   *     leakedPasswords
   * @return {!string}
   * @private
   */
  getStatusIcon_(status, leakedPasswords) {
    if (!this.hasLeaksOrErrors_(status, leakedPasswords)) {
      return 'settings:check-circle';
    }
    if (this.hasLeakedCredentials_(leakedPasswords)) {
      return 'cr:warning';
    }
    return 'cr:info';
  },

  /**
   * Returns the CSS class used to style the icon (warning, info or error).
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @param {!Array<!PasswordManagerProxy.CompromisedCredential>}
   *     leakedPasswords
   * @return {!string}
   * @private
   */
  getStatusIconClass_(status, leakedPasswords) {
    if (!this.hasLeaksOrErrors_(status, leakedPasswords)) {
      return 'no-leaks';
    }
    if (this.hasLeakedCredentials_(leakedPasswords)) {
      return 'has-leaks';
    }
    return '';
  },

  /**
   * Returns the title message indicating the state of the last/ongoing check.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @return {string}
   * @private
   */
  getTitle_(status) {
    switch (status.state) {
      case CheckState.IDLE:
        return this.i18n('checkPasswords');
      case CheckState.CANCELED:
        return this.i18n('checkPasswordsCanceled');
      case CheckState.RUNNING:
        return this.i18n(
            'checkPasswordsProgress', status.alreadyProcessed || 0,
            status.remainingInQueue + status.alreadyProcessed);
      case CheckState.OFFLINE:
        return this.i18n('checkPasswordsErrorOffline');
      case CheckState.SIGNED_OUT:
        return this.i18n('checkPasswordsErrorSignedOut');
      case CheckState.NO_PASSWORDS:
        return this.i18n('checkPasswordsErrorNoPasswords');
      case CheckState.TOO_MANY_PASSWORDS:
        return this.i18n('checkPasswordsErrorTooManyPasswords');
      case CheckState.QUOTA_LIMIT:
        return this.i18n('checkPasswordsErrorQuota');
      case CheckState.OTHER_ERROR:
        return this.i18n('checkPasswordsErrorGeneric');
    }
    throw 'Can\'t find a title for state: ' + status.state;
  },

  /**
   * Returns true iff a check is running right according to the given |status|.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @return {boolean}
   * @private
   */
  isCheckInProgress_(status) {
    return status.state == CheckState.RUNNING;
  },

  /**
   * Returns true to show the timestamp when a check was completed successfully.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @return {boolean}
   * @private
   */
  showsTimestamp_(status) {
    return status.state == CheckState.IDLE &&
        !!status.elapsedTimeSinceLastCheck;
  },

  /**
   * Returns the button caption indicating it's current functionality.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @return {string}
   * @private
   */
  getButtonText_(status) {
    switch (status.state) {
      case CheckState.IDLE:
      case CheckState.CANCELED:
        return this.i18n('checkPasswordsAgain');
      case CheckState.RUNNING:
        return this.i18n('checkPasswordsStop');
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.NO_PASSWORDS:
      case CheckState.OTHER_ERROR:
        return this.i18n('checkPasswordsAgainAfterError');
      case CheckState.QUOTA_LIMIT:
      case CheckState.TOO_MANY_PASSWORDS:
        return '';  // Undefined behavior. Don't show any misleading text.
    }
    throw 'Can\'t find a button text for state: ' + status.state;
  },

  /**
   * Returns true iff the check/stop button should be visible for a given state.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @return {!boolean}
   * @private
   */
  isButtonHidden_(status) {
    switch (status.state) {
      case CheckState.IDLE:
      case CheckState.CANCELED:
      case CheckState.RUNNING:
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.NO_PASSWORDS:
      case CheckState.OTHER_ERROR:
        return false;
      case CheckState.TOO_MANY_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
        return true;
    }
    throw 'Can\'t determine button visibility for state: ' + status.state;
  },

  /**
   * Returns true iff the backend communicated that there are too many saved
   * passwords to check them locally.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @return {boolean}
   * @private
   */
  hasTooManyPasswords_(status) {
    return status.state == CheckState.TOO_MANY_PASSWORDS;
  },

  /**
   * Returns true if there are leaked credentials or the status is unexpected
   * for a regular password check.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @param {!Array<PasswordManagerProxy.CompromisedCredential>}
   *     leakedPasswords
   * @return {boolean}
   * @private
   */
  hasLeaksOrErrors_(status, leakedPasswords) {
    if (this.hasLeakedCredentials_(leakedPasswords)) {
      return true;
    }
    switch (status.state) {
      case CheckState.IDLE:
      case CheckState.RUNNING:
        return false;
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.NO_PASSWORDS:
      case CheckState.TOO_MANY_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
      case CheckState.OTHER_ERROR:
        return true;
    }
    throw 'Not specified whether to state is an error: ' + status.state;
  },

  /**
   * Returns true if there are leaked credentials or the status is unexpected
   * for a regular password check.
   * @param {!PasswordManagerProxy.PasswordCheckStatus} status
   * @param {!Array<PasswordManagerProxy.CompromisedCredential>}
   *     leakedPasswords
   * @return {boolean}
   * @private
   */
  showsPasswordsCount_(status, leakedPasswords) {
    switch (status.state) {
      case CheckState.IDLE:
        return true;
      case CheckState.CANCELED:
      case CheckState.RUNNING:
        return this.hasLeakedCredentials_(leakedPasswords);
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.NO_PASSWORDS:
      case CheckState.TOO_MANY_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
      case CheckState.OTHER_ERROR:
        return false;
    }
    throw 'Not specified whether to show passwords for state: ' + status.state;
  },
});
})();
