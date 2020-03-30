// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

const CheckState = chrome.passwordsPrivate.PasswordCheckState;

Polymer({
  is: 'settings-password-check',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

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

    // <if expr="not chromeos">
    /** @private */
    storedAccounts_: Array,
    // </if>

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
    },

    /** @private */
    suppressCheckupLink_: {
      type: Boolean,
      computed: 'suppressesCheckupLink_(status_, syncPrefs_, syncStatus_)',
    },

    /** @private */
    isSignedOut_: {
      type: Boolean,
      computed: 'computeIsSignedOut_(syncStatus_, storedAccounts_)',
    },

    /** @private */
    isButtonHidden_: {
      type: Boolean,
      computed: 'computeIsButtonHidden_(status_, isSignedOut_)',
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
      computed:
          'computeShowNoCompromisedPasswordsLabel(syncStatus_, prefs.*, status_, leakedPasswords)',
    },

    /** @private */
    showHideMenuTitle_: {
      type: String,
      computed: 'computeShowHideMenuTitle(activePassword_)',
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

  /**
   * The password_check_list_item that the user is interacting with now.
   * @private {?EventTarget}
   */
  activeListItem_: null,

  /** @override */
  attached() {
    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();
    const syncBrowserProxy = settings.SyncBrowserProxyImpl.getInstance();

    const statusChangeListener = status => this.status_ = status;
    const setLeakedCredentialsListener = compromisedCredentials => {
      this.updateList_(compromisedCredentials);

      settings.PluralStringProxyImpl.getInstance()
          .getPluralString('compromisedPasswords', this.leakedPasswords.length)
          .then(count => {
            this.compromisedPasswordsCount_ = count;
          });
    };
    // TODO(crbug.com/1047726): Deduplicate with updates in password_section.js.
    const syncStatusChanged = syncStatus => this.syncStatus_ = syncStatus;
    const syncPrefsChanged = syncPrefs => this.syncPrefs_ = syncPrefs;

    this.statusChangedListener_ = statusChangeListener;
    this.leakedCredentialsListener_ = setLeakedCredentialsListener;

    // Request initial data.
    this.passwordManager_.getPasswordCheckStatus().then(
        this.statusChangedListener_);
    this.passwordManager_.getCompromisedCredentials().then(
        this.leakedCredentialsListener_);
    syncBrowserProxy.getSyncStatus().then(syncStatusChanged);

    // Listen for changes.
    this.passwordManager_.addPasswordCheckStatusListener(
        this.statusChangedListener_);
    this.passwordManager_.addCompromisedCredentialsListener(
        this.leakedCredentialsListener_);
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
      this.passwordManager_.recordPasswordCheckInteraction(
          PasswordManagerProxy.PasswordCheckInteraction
              .START_CHECK_AUTOMATICALLY);
      this.passwordManager_.startBulkPasswordCheck();
    }
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
        this.passwordManager_.recordPasswordCheckInteraction(
            PasswordManagerProxy.PasswordCheckInteraction.STOP_CHECK);
        this.passwordManager_.stopBulkPasswordCheck();
        return;
      case CheckState.IDLE:
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.OTHER_ERROR:
        this.passwordManager_.recordPasswordCheckInteraction(
            PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY);
        this.passwordManager_.startBulkPasswordCheck();
        return;
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
    }
    assertNotReached(
        'Can\'t trigger an action for state: ' + this.status_.state);
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
    this.passwordManager_
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
  getTitle_() {
    switch (this.status_.state) {
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
            'checkPasswordsProgress', (this.status_.alreadyProcessed || 0) + 1,
            Math.max(
                this.status_.remainingInQueue + this.status_.alreadyProcessed,
                1));
      case CheckState.OFFLINE:
        return this.i18n('checkPasswordsErrorOffline');
      case CheckState.SIGNED_OUT:
        return this.i18n('checkPasswordsErrorSignedOut');
      case CheckState.NO_PASSWORDS:
        return this.i18n('checkPasswordsErrorNoPasswords');
      case CheckState.QUOTA_LIMIT:
        return this.i18n('checkPasswordsErrorQuota');
      case CheckState.OTHER_ERROR:
        return this.i18n('checkPasswordsErrorGeneric');
    }
    assertNotReached('Can\'t find a title for state: ' + this.status_.state);
  },

  /**
   * Returns true iff a check is running right according to the given |status|.
   * @return {boolean}
   * @private
   */
  isCheckInProgress_() {
    return this.status_.state == CheckState.RUNNING;
  },

  /**
   * Returns true to show the timestamp when a check was completed successfully.
   * @return {boolean}
   * @private
   */
  showsTimestamp_() {
    return this.status_.state == CheckState.IDLE &&
        !!this.status_.elapsedTimeSinceLastCheck;
  },

  /**
   * Returns the button caption indicating it's current functionality.
   * @return {string}
   * @private
   */
  getButtonText_() {
    switch (this.status_.state) {
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
        'Can\'t find a button text for state: ' + this.status_.state);
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
    switch (this.status_.state) {
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
        'Can\'t determine button visibility for state: ' + this.status_.state);
  },

  /**
   * Returns the chrome:// address where the banner image is located.
   * @param {boolean} isDarkMode
   * @return {string}
   * @private
   */
  bannerImageSrc_(isDarkMode) {
    const type =
        (this.status_.state == CheckState.IDLE && !this.waitsForFirstCheck_()) ?
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
    return this.status_.state == CheckState.CANCELED ||
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
    switch (this.status_.state) {
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
        'Not specified whether to state is an error: ' + this.status_.state);
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
    switch (this.status_.state) {
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
        this.status_.state);
  },

  /**
   * Returns true iff the leak check was performed at least once before.
   * @return {boolean}
   * @private
   */
  waitsForFirstCheck_() {
    return !this.status_.elapsedTimeSinceLastCheck;
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
   * Returns true iff the user cannot retry the password check in their account.
   * Either because they are syncing or because they use a custom passphrase.
   * @return {boolean}
   * @private
   */
  suppressesCheckupLink_() {
    if (this.status_.state != CheckState.QUOTA_LIMIT) {
      return true;  // Never show the retry link for other states.
    }
    if (!this.syncStatus_ || !this.syncStatus_.signedIn) {
      return true;  // Never show the retry link for signed-out users.
    }
    // Show the retry link only, if user data is unencrypted.
    return !!this.syncPrefs_ && !!this.syncPrefs_.encryptAllData;
  },

  /**
   * Function to update compromised credentials in a proper way. New entities
   * should appear in the bottom.
   * @param {!Array<!PasswordManagerProxy.CompromisedCredential>} newList
   * @private
   */
  updateList_(newList) {
    const oldList = this.leakedPasswords.slice();
    const map = new Map();
    newList.forEach(item => map.set(item.id, item));

    const resultList = [];

    oldList.forEach((item, index) => {
      // If element is present in newList
      if (map.has(item.id)) {
        // Replace old version with new
        resultList.push(map.get(item.id));
        resultList[resultList.length - 1].password = item.password;
        map.delete(item.id);
      }
    });

    const addedResults = Array.from(map.values());
    addedResults.sort((lhs, rhs) => {
      // Phished passwords are always shown above
      if (lhs.compromiseType != rhs.compromiseType) {
        return lhs.compromiseType ==
                chrome.passwordsPrivate.CompromiseType.PHISHED ? -1 : 1;
      }
      return rhs.compromiseTime - lhs.compromiseTime;
    });
    addedResults.forEach(item => resultList.push(item));
    this.leakedPasswords = resultList;
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
