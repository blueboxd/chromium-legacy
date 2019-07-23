// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
Polymer({
  is: 'os-settings-people-page',

  behaviors: [
    settings.RouteObserverBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
    LockStateBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * This flag is used to conditionally show a set of sync UIs to the
     * profiles that have been migrated to have a unified consent flow.
     * TODO(tangltom): In the future when all profiles are completely migrated,
     * this should be removed, and UIs hidden behind it should become default.
     * @private
     */
    unifiedConsentEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('unifiedConsentEnabled');
      },
    },

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * Dictionary defining page visibility.
     * @type {!PeoplePageVisibility}
     */
    pageVisibility: Object,

    /**
     * Authentication token provided by settings-lock-screen.
     * @private
     */
    authToken_: {
      type: String,
      value: '',
    },

    /**
     * The current profile name.
     * @private
     */
    profileName_: String,

    /** @private */
    profileLabel_: String,

    /** @private */
    showSignoutDialog_: Boolean,

    /**
     * True if fingerprint settings should be displayed on this machine.
     * @private
     */
    fingerprintUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('fingerprintUnlockEnabled');
      },
      readOnly: true,
    },

    /**
     * True if Chrome OS Account Manager is enabled.
     * @private
     */
    isAccountManagerEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isAccountManagerEnabled');
      },
      readOnly: true,
    },

    /**
     * True if Chrome OS Kerberos support is enabled.
     * @private
     */
    isKerberosEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isKerberosEnabled');
      },
      readOnly: true,
    },

    /** @private */
    showParentalControls_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('showParentalControls') &&
            loadTimeData.getBoolean('showParentalControls');
      },
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.SYNC) {
          map.set(
              settings.routes.SYNC.path,
              loadTimeData.getBoolean('unifiedConsentEnabled') ?
                  '#sync-setup' :
                  '#sync-status .subpage-arrow');
        }
        if (settings.routes.LOCK_SCREEN) {
          map.set(
              settings.routes.LOCK_SCREEN.path, '#lock-screen-subpage-trigger');
        }
        if (settings.routes.ACCOUNTS) {
          map.set(
              settings.routes.ACCOUNTS.path,
              '#manage-other-people-subpage-trigger');
        }
        if (settings.routes.ACCOUNT_MANAGER) {
          map.set(
              settings.routes.ACCOUNT_MANAGER.path,
              '#account-manager-subpage-trigger');
        }
        if (settings.routes.KERBEROS_ACCOUNTS) {
          map.set(
              settings.routes.KERBEROS_ACCOUNTS.path,
              '#kerberos-accounts-subpage-trigger');
        }
        return map;
      },
    },
  },

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @private {?settings.AccountManagerBrowserProxy} */
  accountManagerBrowserProxy_: null,

  /** @override */
  attached: function() {
    const profileInfoProxy = settings.ProfileInfoBrowserProxyImpl.getInstance();
    profileInfoProxy.getProfileInfo().then(this.handleProfileInfo_.bind(this));
    this.addWebUIListener(
        'profile-info-changed', this.handleProfileInfo_.bind(this));

    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));

    this.accountManagerBrowserProxy_ =
        settings.AccountManagerBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'accounts-changed', this.updateProfileLabel_.bind(this));
    this.updateProfileLabel_();
  },

  /** @protected */
  currentRouteChanged: function() {
    if (settings.getCurrentRoute() == settings.routes.SIGN_OUT) {
      // If the sync status has not been fetched yet, optimistically display
      // the sign-out dialog. There is another check when the sync status is
      // fetched. The dialog will be closed when the user is not signed in.
      if (this.syncStatus && !this.syncStatus.signedIn) {
        settings.navigateToPreviousRoute();
      } else {
        this.showSignoutDialog_ = true;
      }
    }
  },

  /** @private */
  getPasswordState_: function(hasPin, enableScreenLock) {
    if (!enableScreenLock) {
      return this.i18n('lockScreenNone');
    }
    if (hasPin) {
      return this.i18n('lockScreenPinOrPassword');
    }
    return this.i18n('lockScreenPasswordOnly');
  },

  /**
   * Handler for when the profile's name is updated.
   * @private
   * @param {!settings.ProfileInfo} info
   */
  handleProfileInfo_: function(info) {
    this.profileName_ = info.name;
    // info.iconUrl is not used for this page.
  },

  /**
   * Updates the label underneath the primary profile name.
   * @private
   */
  updateProfileLabel_: async function() {
    const includeImages = false;
    const /** @type {!Array<settings.Account>} */ accounts =
        await this.accountManagerBrowserProxy_.getAccounts(includeImages);
    // The user might not have any GAIA accounts.
    if (accounts.length == 0) {
      this.profileLabel_ = '';
      return;
    }
    const moreAccounts = accounts.length - 1;
    // Template: "$1, +$2 more accounts" with correct plural of "account".
    // Localization handles the case of 0 more accounts.
    const labelTemplate = await cr.sendWithPromise(
        'getPluralString', 'profileLabel', moreAccounts);

    // Final output: "alice@gmail.com, +2 more accounts"
    this.profileLabel_ = loadTimeData.substituteString(
        labelTemplate, accounts[0].email, moreAccounts);
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    this.syncStatus = syncStatus;
  },

  /** @private */
  onSigninTap_: function() {
    this.syncBrowserProxy_.startSignIn();
  },

  /** @private */
  onDisconnectDialogClosed_: function(e) {
    this.showSignoutDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#disconnectButton')));

    if (settings.getCurrentRoute() == settings.routes.SIGN_OUT) {
      settings.navigateToPreviousRoute();
    }
  },

  /** @private */
  onDisconnectTap_: function() {
    settings.navigateTo(settings.routes.SIGN_OUT);
  },

  /** @private */
  onSyncTap_: function() {
    // When unified-consent is enabled, users can go to sync subpage regardless
    // of sync status.
    if (this.unifiedConsentEnabled_) {
      settings.navigateTo(settings.routes.SYNC);
      return;
    }

    // TODO(crbug.com/862983): Remove this code once UnifiedConsent is rolled
    // out to 100%.
    assert(this.syncStatus.signedIn);
    assert(this.syncStatus.syncSystemEnabled);

    if (!this.isSyncStatusActionable_(this.syncStatus)) {
      return;
    }

    switch (this.syncStatus.statusAction) {
      case settings.StatusAction.REAUTHENTICATE:
        this.syncBrowserProxy_.startSignIn();
        break;
      case settings.StatusAction.SIGNOUT_AND_SIGNIN:
        this.syncBrowserProxy_.attemptUserExit();
        break;
      case settings.StatusAction.UPGRADE_CLIENT:
        settings.navigateTo(settings.routes.ABOUT);
        break;
      case settings.StatusAction.ENTER_PASSPHRASE:
      case settings.StatusAction.CONFIRM_SYNC_SETTINGS:
      case settings.StatusAction.NO_ACTION:
      default:
        settings.navigateTo(settings.routes.SYNC);
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigureLockTap_: function(e) {
    // Navigating to the lock screen will always open the password prompt
    // dialog, so prevent the end of the tap event to focus what is underneath
    // it, which takes focus from the dialog.
    e.preventDefault();
    settings.navigateTo(settings.routes.LOCK_SCREEN);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAccountManagerTap_: function(e) {
    settings.navigateTo(settings.routes.ACCOUNT_MANAGER);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKerberosAccountsTap_: function(e) {
    settings.navigateTo(settings.routes.KERBEROS_ACCOUNTS);
  },

  /** @private */
  onManageOtherPeople_: function() {
    settings.navigateTo(settings.routes.ACCOUNTS);
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {boolean}
   */
  isPreUnifiedConsentAdvancedSyncSettingsVisible_: function(syncStatus) {
    return !!syncStatus && !!syncStatus.signedIn &&
        !!syncStatus.syncSystemEnabled && !this.unifiedConsentEnabled_;
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {boolean}
   */
  isAdvancedSyncSettingsSearchable_: function(syncStatus) {
    return this.isPreUnifiedConsentAdvancedSyncSettingsVisible_(syncStatus) ||
        !!this.unifiedConsentEnabled_;
  },

  /**
   * @private
   * @return {Element|null}
   */
  getAdvancedSyncSettingsAssociatedControl_: function() {
    return this.unifiedConsentEnabled_ ? this.$$('#sync-setup') :
                                         this.$$('#sync-status');
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {boolean} Whether an action can be taken with the sync status. sync
   *     status is actionable if sync is not managed and if there is a sync
   *     error, there is an action associated with it.
   */
  isSyncStatusActionable_: function(syncStatus) {
    return !!syncStatus && !syncStatus.managed &&
        (!syncStatus.hasError ||
         syncStatus.statusAction != settings.StatusAction.NO_ACTION);
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {string}
   */
  getSyncIcon_: function(syncStatus) {
    if (!syncStatus) {
      return '';
    }

    let syncIcon = 'cr:sync';

    if (syncStatus.hasError) {
      syncIcon = 'settings:sync-problem';
    }

    // Override the icon to the disabled icon if sync is managed.
    if (syncStatus.managed ||
        syncStatus.statusAction == settings.StatusAction.REAUTHENTICATE) {
      syncIcon = 'settings:sync-disabled';
    }

    return syncIcon;
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {string} The class name for the sync status row.
   */
  getSyncStatusClass_: function(syncStatus) {
    if (syncStatus && syncStatus.hasError) {
      // Most of the time re-authenticate states are caused by intentional user
      // action, so they will be displayed differently as other errors.
      return syncStatus.statusAction == settings.StatusAction.REAUTHENTICATE ?
          'auth-error' :
          'sync-error';
    }

    return '';
  },

  /**
   * @param {!settings.SyncStatus} syncStatus
   * @return {boolean} Whether to show the "Sign in to Chrome" button.
   * @private
   */
  showSignin_: function(syncStatus) {
    return !!syncStatus.signinAllowed && !syncStatus.signedIn;
  },

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @private
   */
  selectLockScreenTitleString(hasPinLogin) {
    if (hasPinLogin) {
      return this.i18n('lockScreenTitleLoginLock');
    }
    return this.i18n('lockScreenTitleLock');
  },
});
