// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

const CheckState = chrome.passwordsPrivate.PasswordCheckState;

Polymer({
  is: 'settings-password-check',

  behaviors: [
    I18nBehavior,
    PasswordCheckBehavior,
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    // <if expr="not chromeos">
    /** @private */
    storedAccounts_: Array,
    // </if>

    /** @private */
    title_: {
      type: String,
      computed: 'computeTitle_(status, canUsePasswordCheckup_)',
    },

    /** @private */
    isSignedOut_: {
      type: Boolean,
      computed: 'computeIsSignedOut_(syncStatus_, storedAccounts_)',
    },

    canUsePasswordCheckup_: {
      type: Boolean,
      computed: 'computeCanUsePasswordCheckup_(syncPrefs_, syncStatus_)',
    },

    /** @private */
    isButtonHidden_: {
      type: Boolean,
      computed: 'computeIsButtonHidden_(status, isSignedOut_)',
    },

    /** @private {settings.SyncPrefs} */
    syncPrefs_: Object,

    /** @private {settings.SyncStatus} */
    syncStatus_: Object,

    /** @private */
    showPasswordEditDialog_: Boolean,

    /** @private */
    showPasswordRemoveDialog_: Boolean,

    /**
     * The password that the user is interacting with now.
     * @private {?PasswordManagerProxy.CompromisedCredential}
     */
    activePassword_: Object,

    /** @private */
    showNoCompromisedPasswordsLabel_: {
      type: Boolean,
      computed: 'computeShowNoCompromisedPasswordsLabel(' +
          'syncStatus_, prefs.*, status, leakedPasswords)',
    },

    /** @private */
    showHideMenuTitle_: {
      type: String,
      computed: 'computeShowHideMenuTitle(activePassword_)',
    }
  },

  /**
   * The element to return focus to, when the currently active dialog is
   * closed.
   * @private {?HTMLElement}
   */
  activeDialogAnchor_: null,

  /**
   * The password_check_list_item that the user is interacting with now.
   * @private {?EventTarget}
   */
  activeListItem_: null,

  /** @override */
  attached() {
    // Set the manager. These can be overridden by tests.
    const syncBrowserProxy = settings.SyncBrowserProxyImpl.getInstance();

    const syncStatusChanged = syncStatus => this.syncStatus_ = syncStatus;
    const syncPrefsChanged = syncPrefs => this.syncPrefs_ = syncPrefs;

    // Request initial data.
    syncBrowserProxy.getSyncStatus().then(syncStatusChanged);

    // Listen for changes.
    this.addWebUIListener('sync-status-changed', syncStatusChanged);
    this.addWebUIListener('sync-prefs-changed', syncPrefsChanged);

    // For non-ChromeOS, also check whether accounts are available.
    // <if expr="not chromeos">
    const storedAccountsChanged = accounts => this.storedAccounts_ = accounts;
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>

    // Start the check if instructed to do so.
    const router = settings.Router.getInstance();
    if (router.getQueryParameters().get('start') == 'true') {
      this.passwordManager.recordPasswordCheckInteraction(
          PasswordManagerProxy.PasswordCheckInteraction
              .START_CHECK_AUTOMATICALLY);
      this.passwordManager.startBulkPasswordCheck();
    }
  },

  /**
   * Start/Stop bulk password check.
   * @private
   */
  onPasswordCheckButtonClick_() {
    switch (this.status.state) {
      case CheckState.RUNNING:
        this.passwordManager.recordPasswordCheckInteraction(
            PasswordManagerProxy.PasswordCheckInteraction.STOP_CHECK);
        this.passwordManager.stopBulkPasswordCheck();
        return;
      case CheckState.IDLE:
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.OTHER_ERROR:
        this.passwordManager.recordPasswordCheckInteraction(
            PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY);
        this.passwordManager.startBulkPasswordCheck();
        return;
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
    }
    assertNotReached(
        'Can\'t trigger an action for state: ' + this.status.state);
  },

  /**
   * Returns true if there are any compromised credentials.
   * @return {boolean}
   * @private
   */
  hasLeakedCredentials_() {
    return !!this.leakedPasswords.length;
  },

  /**
   * @param {!CustomEvent<{moreActionsButton: !HTMLElement}>} event
   * @private
   */
  onMoreActionsClick_(event) {
    const target = event.detail.moreActionsButton;
    this.$.moreActionsMenu.showAt(target);
    this.activeDialogAnchor_ = target;
    this.activeListItem_ = event.target;
    this.activePassword_ = this.activeListItem_.item;
  },

  /** @private */
  onMenuShowPasswordClick_() {
    this.activePassword_.password ? this.activeListItem_.hidePassword() :
                                    this.activeListItem_.showPassword();
    this.$.moreActionsMenu.close();
    this.activePassword_ = null;
    this.activeDialogAnchor_ = null;
  },

  /** @private */
  onMenuEditPasswordClick_() {
    this.passwordManager
        .getPlaintextCompromisedPassword(
            assert(this.activePassword_),
            chrome.passwordsPrivate.PlaintextReason.EDIT)
        .then(
            compromisedCredential => {
              this.activePassword_ = compromisedCredential;
              this.$.moreActionsMenu.close();
              this.showPasswordEditDialog_ = true;
            },
            error => {
              this.activePassword_ = null;
              this.$.moreActionsMenu.close();
              this.onPasswordEditDialogClosed_();
            });
  },

  /** @private */
  onMenuRemovePasswordClick_() {
    this.$.moreActionsMenu.close();
    this.showPasswordRemoveDialog_ = true;
  },

  /** @private */
  onPasswordRemoveDialogClosed_() {
    this.showPasswordRemoveDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
  },

  /** @private */
  onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
  },

  computeShowHideMenuTitle() {
    return this.i18n(
        this.activeListItem_.isPasswordVisible_ ? 'hideCompromisedPassword' :
                                                  'showCompromisedPassword');
  },

  /**
   * Returns the icon (warning, info or error) indicating the check status.
   * @return {string}
   * @private
   */
  getStatusIcon_() {
    if (!this.hasLeaksOrErrors_()) {
      return 'settings:check-circle';
    }
    if (this.hasLeakedCredentials_()) {
      return 'cr:warning';
    }
    return 'cr:info';
  },

  /**
   * Returns the CSS class used to style the icon (warning, info or error).
   * @return {string}
   * @private
   */
  getStatusIconClass_() {
    if (!this.hasLeaksOrErrors_()) {
      return this.waitsForFirstCheck_() ? 'hidden' : 'no-leaks';
    }
    if (this.hasLeakedCredentials_()) {
      return 'has-leaks';
    }
    return '';
  },

  /**
   * Returns the title message indicating the state of the last/ongoing check.
   * @return {string}
   * @private
   */
  computeTitle_() {
    switch (this.status.state) {
      case CheckState.IDLE:
        return this.waitsForFirstCheck_() ?
            this.i18n('checkPasswordsDescription') :
            this.i18n('checkedPasswords');
      case CheckState.CANCELED:
        return this.i18n('checkPasswordsCanceled');
      case CheckState.RUNNING:
        // Returns the progress of a running check. Ensures that both numbers
        // are at least 1.
        return this.i18n(
            'checkPasswordsProgress', (this.status.alreadyProcessed || 0) + 1,
            Math.max(
                this.status.remainingInQueue + this.status.alreadyProcessed,
                1));
      case CheckState.OFFLINE:
        return this.i18n('checkPasswordsErrorOffline');
      case CheckState.SIGNED_OUT:
        return this.i18n('checkPasswordsErrorSignedOut');
      case CheckState.NO_PASSWORDS:
        return this.i18n('checkPasswordsErrorNoPasswords');
      case CheckState.QUOTA_LIMIT:
        // Note: For the checkup case we embed the link as HTML, thus we need to
        // use i18nAdvanced() here as well as the `inner-h-t-m-l` attribute in
        // the DOM.
        return this.canUsePasswordCheckup_ ?
            this.i18nAdvanced('checkPasswordsErrorQuotaGoogleAccount') :
            this.i18n('checkPasswordsErrorQuota');
      case CheckState.OTHER_ERROR:
        return this.i18n('checkPasswordsErrorGeneric');
    }
    assertNotReached('Can\'t find a title for state: ' + this.status.state);
  },

  /**
   * Returns true iff a check is running right according to the given |status|.
   * @return {boolean}
   * @private
   */
  isCheckInProgress_() {
    return this.status.state == CheckState.RUNNING;
  },

  /**
   * Returns true to show the timestamp when a check was completed successfully.
   * @return {boolean}
   * @private
   */
  showsTimestamp_() {
    return this.status.state == CheckState.IDLE &&
        !!this.status.elapsedTimeSinceLastCheck;
  },

  /**
   * Returns the button caption indicating it's current functionality.
   * @return {string}
   * @private
   */
  getButtonText_() {
    switch (this.status.state) {
      case CheckState.IDLE:
        return this.waitsForFirstCheck_() ? this.i18n('checkPasswords') :
                                            this.i18n('checkPasswordsAgain');
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
        return '';  // Undefined behavior. Don't show any misleading text.
    }
    assertNotReached(
        'Can\'t find a button text for state: ' + this.status.state);
  },

  /**
   * Returns 'action-button' only for the very first check.
   * @return {string}
   * @private
   */
  getButtonTypeClass_() {
    return this.waitsForFirstCheck_() ? 'action-button' : ' ';
  },

  /**
   * Returns true iff the check/stop button should be visible for a given state.
   * @return {!boolean}
   * @private
   */
  computeIsButtonHidden_() {
    switch (this.status.state) {
      case CheckState.IDLE:
      case CheckState.CANCELED:
      case CheckState.RUNNING:
      case CheckState.OFFLINE:
      case CheckState.OTHER_ERROR:
        return false;
      case CheckState.SIGNED_OUT:
        return this.isSignedOut_;
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
        return true;
    }
    assertNotReached(
        'Can\'t determine button visibility for state: ' + this.status.state);
  },

  /**
   * Returns the chrome:// address where the banner image is located.
   * @param {boolean} isDarkMode
   * @return {string}
   * @private
   */
  bannerImageSrc_(isDarkMode) {
    const type =
        (this.status.state == CheckState.IDLE && !this.waitsForFirstCheck_()) ?
        'positive' :
        'neutral';
    const suffix = isDarkMode ? '_dark' : '';
    return `chrome://settings/images/password_check_${type}${suffix}.svg`;
  },

  /**
   * Returns true iff the banner should be shown.
   * @return {boolean}
   * @private
   */
  shouldShowBanner_() {
    if (this.hasLeakedCredentials_()) {
      return false;
    }
    return this.status.state == CheckState.CANCELED ||
        !this.hasLeaksOrErrors_();
  },

  /**
   * Returns true if there are leaked credentials or the status is unexpected
   * for a regular password check.
   * @return {boolean}
   * @private
   */
  hasLeaksOrErrors_() {
    if (this.hasLeakedCredentials_()) {
      return true;
    }
    switch (this.status.state) {
      case CheckState.IDLE:
      case CheckState.RUNNING:
        return false;
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
      case CheckState.OTHER_ERROR:
        return true;
    }
    assertNotReached(
        'Not specified whether to state is an error: ' + this.status.state);
  },

  /**
   * Returns true if there are leaked credentials or the status is unexpected
   * for a regular password check.
   * @return {boolean}
   * @private
   */
  showsPasswordsCount_() {
    if (this.hasLeakedCredentials_()) {
      return true;
    }
    switch (this.status.state) {
      case CheckState.IDLE:
        return !this.waitsForFirstCheck_();
      case CheckState.CANCELED:
      case CheckState.RUNNING:
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
      case CheckState.OTHER_ERROR:
        return false;
    }
    assertNotReached(
        'Not specified whether to show passwords for state: ' +
        this.status.state);
  },

  /**
   * Returns true iff the leak check was performed at least once before.
   * @return {boolean}
   * @private
   */
  waitsForFirstCheck_() {
    return !this.status.elapsedTimeSinceLastCheck;
  },

  /**
   * Returns true iff the user is signed in.
   * @return {boolean}
   * @private
   */
  computeIsSignedOut_() {
    if (!this.syncStatus_ || !this.syncStatus_.signedIn) {
      return !this.storedAccounts_ || this.storedAccounts_.length == 0;
    }
    return !!this.syncStatus_.hasError;
  },

  /**
   * Returns whether the user can use the online Password Checkup.
   * @return {boolean}
   * @private
   */
  computeCanUsePasswordCheckup_() {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn &&
        (!this.syncPrefs_ || !this.syncPrefs_.encryptAllData);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowNoCompromisedPasswordsLabel() {
    // Check if user isn't signed in.
    if (!this.syncStatus_ || !this.syncStatus_.signedIn) {
      return false;
    }

    // Check if breach detection is turned off in settings.
    if (!this.prefs ||
        !this.getPref('profile.password_manager_leak_detection').value) {
      return false;
    }

    // Return true if there was a successful check and no compromised passwords
    // were found.
    return !this.hasLeakedCredentials_() && this.showsTimestamp_();
  },
});
})();
