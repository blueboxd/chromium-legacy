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

    splitSettingsSyncEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('splitSettingsSyncEnabled');
      },
    },

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * Dictionary defining page visibility.
     * @type {!OSPageVisibility}
     */
    pageVisibility: Object,

    /**
     * Authentication token provided by settings-lock-screen.
     * @private {!chrome.quickUnlockPrivate.TokenInfo|undefined}
     */
    authToken_: {
      type: Object,
      observer: 'onAuthTokenChanged_',
    },

    /**
     * The current profile icon URL. Usually a data:image/png URL.
     * @private
     */
    profileIconUrl_: String,

    /**
     * The current profile name, e.g. "John Cena".
     * @private
     */
    profileName_: String,

    /**
     * The current profile email, e.g. "john.cena@gmail.com".
     * @private
     */
    profileEmail_: String,

    /**
     * The label may contain additional text, for example:
     * "john.cena@gmail, + 2 more accounts".
     * @private
     */
    profileLabel_: String,

    /** @private */
    showSignoutDialog_: Boolean,

    /**
     * True if fingerprint settings should be displayed on this machine.
     * @private
     */
    fingerprintUnlockEnabled_: {
      type: Boolean,
      value() {
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
      value() {
        return loadTimeData.getBoolean('isAccountManagerEnabled');
      },
      readOnly: true,
    },

    /** @private */
    showParentalControls_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('showParentalControls') &&
            loadTimeData.getBoolean('showParentalControls');
      },
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.SYNC) {
          map.set(settings.routes.SYNC.path, '#sync-setup');
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

  /** @override */
  attached() {
    if (this.isAccountManagerEnabled_) {
      // If we have the Google Account manager, use GAIA name and icon.
      this.addWebUIListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    } else {
      // Otherwise use the Profile name and icon.
      settings.ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
          this.handleProfileInfo_.bind(this));
      this.addWebUIListener(
          'profile-info-changed', this.handleProfileInfo_.bind(this));
    }

    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  },

  /** @protected */
  currentRouteChanged() {
    if (settings.Router.getInstance().getCurrentRoute() ==
        settings.routes.OS_SIGN_OUT) {
      // If the sync status has not been fetched yet, optimistically display
      // the sign-out dialog. There is another check when the sync status is
      // fetched. The dialog will be closed when the user is not signed in.
      if (this.syncStatus && !this.syncStatus.signedIn) {
        settings.Router.getInstance().navigateToPreviousRoute();
      } else {
        this.showSignoutDialog_ = true;
      }
    }
  },

  /** @private */
  getPasswordState_(hasPin, enableScreenLock) {
    if (!enableScreenLock) {
      return this.i18n('lockScreenNone');
    }
    if (hasPin) {
      return this.i18n('lockScreenPinOrPassword');
    }
    return this.i18n('lockScreenPasswordOnly');
  },

  /**
   * @return {string}
   * @private
   */
  getSyncRowLabel_() {
    if (this.splitSettingsSyncEnabled_) {
      return this.i18n('osSyncPageTitle');
    } else {
      return this.i18n('syncAndNonPersonalizedServices');
    }
  },

  /**
   * @return {string}
   * @private
   */
  getSyncAndGoogleServicesSubtext_() {
    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return this.syncStatus.statusText;
    }
    return '';
  },

  /**
   * Handler for when the profile's icon and name is updated.
   * @private
   * @param {!settings.ProfileInfo} info
   */
  handleProfileInfo_(info) {
    this.profileName_ = info.name;
    // Extract first frame from image by creating a single frame PNG using
    // url as input if base64 encoded and potentially animated.
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ = cr.png.convertImageSequenceToPng([info.iconUrl]);
      return;
    }
    this.profileIconUrl_ = info.iconUrl;
  },

  /**
   * Handler for when the account list is updated.
   * @private
   */
  updateAccounts_: async function() {
    const /** @type {!Array<settings.Account>} */ accounts =
        await settings.AccountManagerBrowserProxyImpl.getInstance()
            .getAccounts();
    // The user might not have any GAIA accounts (e.g. guest mode, Kerberos,
    // Active Directory). In these cases the profile row is hidden, so there's
    // nothing to do.
    if (accounts.length == 0) {
      return;
    }
    this.profileName_ = accounts[0].fullName;
    this.profileEmail_ = accounts[0].email;
    this.profileIconUrl_ = accounts[0].pic;

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
  handleSyncStatus_(syncStatus) {
    this.syncStatus = syncStatus;

    // When ChromeOSAccountManager is disabled, fall back to using the sync
    // username ("alice@gmail.com") as the profile label.
    if (!this.isAccountManagerEnabled_ && syncStatus && syncStatus.signedIn &&
        syncStatus.signedInUsername) {
      this.profileLabel_ = syncStatus.signedInUsername;
    }
  },

  /** @private */
  onSigninTap_() {
    this.syncBrowserProxy_.startSignIn();
  },

  /** @private */
  onDisconnectDialogClosed_(e) {
    this.showSignoutDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#disconnectButton')));

    if (settings.Router.getInstance().getCurrentRoute() ==
        settings.routes.OS_SIGN_OUT) {
      settings.Router.getInstance().navigateToPreviousRoute();
    }
  },

  /** @private */
  onDisconnectTap_() {
    settings.Router.getInstance().navigateTo(settings.routes.OS_SIGN_OUT);
  },

  /** @private */
  onSyncTap_() {
    if (this.splitSettingsSyncEnabled_) {
      settings.Router.getInstance().navigateTo(settings.routes.OS_SYNC);
      return;
    }

    // Users can go to sync subpage regardless of sync status.
    settings.Router.getInstance().navigateTo(settings.routes.SYNC);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigureLockTap_(e) {
    // Navigating to the lock screen will always open the password prompt
    // dialog, so prevent the end of the tap event to focus what is underneath
    // it, which takes focus from the dialog.
    e.preventDefault();
    settings.Router.getInstance().navigateTo(settings.routes.LOCK_SCREEN);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAccountManagerTap_(e) {
    if (this.isAccountManagerEnabled_) {
      settings.Router.getInstance().navigateTo(settings.routes.ACCOUNT_MANAGER);
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKerberosAccountsTap_(e) {
    settings.Router.getInstance().navigateTo(settings.routes.KERBEROS_ACCOUNTS);
  },

  /** @private */
  onManageOtherPeople_() {
    settings.Router.getInstance().navigateTo(settings.routes.ACCOUNTS);
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!settings.SyncStatus} syncStatus
   * @return {boolean} Whether to show the "Sign in to Chrome" button.
   * @private
   */
  showSignin_(syncStatus) {
    return loadTimeData.getBoolean('signinAllowed') && !syncStatus.signedIn;
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

  /**
   * The timeout ID to pass to clearTimeout() to cancel auth token
   * invalidation.
   * @private {number|undefined}
   */
  clearAccountPasswordTimeoutId_: undefined,

  /** @private */
  onAuthTokenChanged_() {
    if (this.clearAuthTokenTimeoutId_) {
      clearTimeout(this.clearAccountPasswordTimeoutId_);
    }
    if (this.authToken_ === undefined) {
      return;
    }
    // Clear |this.authToken_| after
    // |this.authToken_.tokenInfo.lifetimeSeconds|.
    // Subtract time from the expiration time to account for IPC delays.
    // Treat values less than the minimum as 0 for testing.
    const IPC_SECONDS = 2;
    const lifetimeMs = this.authToken_.lifetimeSeconds > IPC_SECONDS ?
        (this.authToken_.lifetimeSeconds - IPC_SECONDS) * 1000 :
        0;
    this.clearAccountPasswordTimeoutId_ = setTimeout(() => {
      this.authToken_ = undefined;
    }, lifetimeMs);
  },
});
