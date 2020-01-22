// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
Polymer({
  is: 'settings-people-page',

  behaviors: [
    settings.RouteObserverBehavior, I18nBehavior, WebUIListenerBehavior,
    // <if expr="chromeos">
    CrPngBehavior,
    // </if>
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    // Chrome OS does not support DICE.
    // <if expr="not chromeos">
    /**
     * This flag is used to conditionally show a set of new sign-in UIs to the
     * profiles that have been migrated to be consistent with the web sign-ins.
     * TODO(tangltom): In the future when all profiles are completely migrated,
     * this should be removed, and UIs hidden behind it should become default.
     * @private
     */
    diceEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('diceEnabled');
      },
    },

    /**
     * Stored accounts to the system, supplied by SyncBrowserProxy.
     * @type {?Array<!settings.StoredAccount>}
     */
    storedAccounts: Object,
    // </if>

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * Dictionary defining page visibility.
     * @type {!PageVisibility}
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
     * The currently selected profile icon URL. May be a data URL.
     * @private
     */
    profileIconUrl_: String,

    /**
     * Whether the profile row is clickable. The behavior depends on the
     * platform.
     * @private
     */
    isProfileActionable_: {
      type: Boolean,
      value() {
        if (!cr.isChromeOS) {
          // Opens profile manager.
          return true;
        }
        // Post-SplitSettings links out to account manager if it is available.
        return loadTimeData.getBoolean('isAccountManagerEnabled');
      },
      readOnly: true,
    },

    /**
     * The current profile name.
     * @private
     */
    profileName_: String,

    // <if expr="not chromeos">
    /** @private {boolean} */
    shouldShowGoogleAccount_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowGoogleAccount_(storedAccounts, syncStatus,' +
          'storedAccounts.length, syncStatus.signedIn, syncStatus.hasError)',
    },

    /** @private */
    showImportDataDialog_: {
      type: Boolean,
      value: false,
    },
    // </if>

    /** @private */
    showSignoutDialog_: Boolean,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.SYNC) {
          map.set(settings.routes.SYNC.path, '#sync-setup');
        }
        // <if expr="not chromeos">
        if (settings.routes.MANAGE_PROFILE) {
          map.set(
              settings.routes.MANAGE_PROFILE.path,
              loadTimeData.getBoolean('diceEnabled') ?
                  '#edit-profile .subpage-arrow' :
                  '#picture-subpage-trigger .subpage-arrow');
        }
        // </if>
        return map;
      },
    },
  },

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached() {
    let useProfileNameAndIcon = true;
    // <if expr="chromeos">
    if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // If this is SplitSettings and we have the Google Account manager,
      // prefer the GAIA name and icon.
      useProfileNameAndIcon = false;
      this.addWebUIListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    }
    // </if>
    if (useProfileNameAndIcon) {
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

    // <if expr="not chromeos">
    const handleStoredAccounts = accounts => {
      this.storedAccounts = accounts;
    };
    this.syncBrowserProxy_.getStoredAccounts().then(handleStoredAccounts);
    this.addWebUIListener('stored-accounts-updated', handleStoredAccounts);

    this.addWebUIListener('sync-settings-saved', () => {
      /** @type {!CrToastElement} */ (this.$.toast).show();
    });
    // </if>
  },

  /** @protected */
  currentRouteChanged() {
    this.showImportDataDialog_ =
        settings.Router.getInstance().getCurrentRoute() ==
        settings.routes.IMPORT_DATA;

    if (settings.Router.getInstance().getCurrentRoute() ==
        settings.routes.SIGN_OUT) {
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

  /**
   * @return {!Element}
   * @private
   */
  getEditPersonAssocControl_() {
    return this.diceEnabled_ ? assert(this.$$('#edit-profile')) :
                               assert(this.$$('#picture-subpage-trigger'));
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
    /**
     * Extract first frame from image by creating a single frame PNG using
     * url as input if base64 encoded and potentially animated.
     */
    // <if expr="chromeos">
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ =
          CrPngBehavior.convertImageSequenceToPng([info.iconUrl]);
      return;
    }
    // </if>

    this.profileIconUrl_ = info.iconUrl;
  },

  // <if expr="chromeos">
  /**
   * @private
   * @suppress {checkTypes} The types only exists in Chrome OS builds, but
   * Closure doesn't understand the <if> above.
   */
  updateAccounts_: async function() {
    const /** @type {!Array<{settings.Account}>} */ accounts =
        await settings.AccountManagerBrowserProxyImpl.getInstance()
            .getAccounts();
    // The user might not have any GAIA accounts (e.g. guest mode, Kerberos,
    // Active Directory). In these cases the profile row is hidden, so there's
    // nothing to do.
    if (accounts.length == 0) {
      return;
    }
    this.profileName_ = accounts[0].fullName;
    this.profileIconUrl_ = accounts[0].pic;
  },
  // </if>

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_(syncStatus) {
    // Sign-in impressions should be recorded only if the sign-in promo is
    // shown. They should be recorder only once, the first time
    // |this.syncStatus| is set.
    const shouldRecordSigninImpression =
        !this.syncStatus && syncStatus && this.showSignin_(syncStatus);

    this.syncStatus = syncStatus;

    if (shouldRecordSigninImpression && !this.shouldShowSyncAccountControl_()) {
      // SyncAccountControl records the impressions user actions.
      chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');
    }
  },

  // <if expr="not chromeos">
  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowGoogleAccount_() {
    if (this.storedAccounts === undefined || this.syncStatus === undefined) {
      return false;
    }

    return (this.storedAccounts.length > 0 || !!this.syncStatus.signedIn) &&
        !this.syncStatus.hasError;
  },
  // </if>

  /** @private */
  onProfileTap_() {
    // <if expr="chromeos">
    if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // Post-SplitSettings. The browser C++ code loads OS settings in a window.
      // Don't use window.open() because that creates an extra empty tab.
      window.location.href = 'chrome://os-settings/accountManager';
    }
    // </if>
    // <if expr="not chromeos">
    settings.Router.getInstance().navigateTo(settings.routes.MANAGE_PROFILE);
    // </if>
  },

  /** @private */
  onSigninTap_() {
    this.syncBrowserProxy_.startSignIn();
  },

  /** @private */
  onDisconnectDialogClosed_(e) {
    this.showSignoutDialog_ = false;
    // <if expr="not chromeos">
    if (!this.diceEnabled_) {
      // If DICE-enabled, this button won't exist here.
      cr.ui.focusWithoutInk(assert(this.$$('#disconnectButton')));
    }
    // </if>

    if (settings.Router.getInstance().getCurrentRoute() ==
        settings.routes.SIGN_OUT) {
      settings.Router.getInstance().navigateToPreviousRoute();
    }
  },

  /** @private */
  onDisconnectTap_() {
    settings.Router.getInstance().navigateTo(settings.routes.SIGN_OUT);
  },

  /** @private */
  onSyncTap_() {
    // Users can go to sync subpage regardless of sync status.
    settings.Router.getInstance().navigateTo(settings.routes.SYNC);
  },

  // <if expr="not chromeos">
  /** @private */
  onImportDataTap_() {
    settings.Router.getInstance().navigateTo(settings.routes.IMPORT_DATA);
  },

  /** @private */
  onImportDataDialogClosed_() {
    settings.Router.getInstance().navigateToPreviousRoute();
    cr.ui.focusWithoutInk(assert(this.$.importDataDialogTrigger));
  },
  // </if>

  /**
   * Open URL for managing your Google Account.
   * @private
   */
  openGoogleAccount_() {
    settings.OpenWindowProxyImpl.getInstance().openURL(
        loadTimeData.getString('googleAccountUrl'));
    chrome.metricsPrivate.recordUserAction('ManageGoogleAccount_Clicked');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSyncAccountControl_() {
    if (this.syncStatus == undefined) {
      return false;
    }
    // <if expr="chromeos">
    if (!loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      return false;
    }
    // </if>
    return !!this.syncStatus.syncSystemEnabled &&
        !!this.syncStatus.signinAllowed;
  },

  /**
   * @return {boolean} Whether to show the profile row and associated controls.
   * @private
   */
  shouldShowProfile_() {
    // Closure compiler doesn't understand <if> so use a variable.
    let show = false;
    // <if expr="chromeos">
    show = !this.shouldShowSyncAccountControl_();
    // </if>
    // <if expr="not chromeos">
    show = !this.diceEnabled_;
    // </if>
    return show;
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
    return !!syncStatus.signinAllowed && !syncStatus.signedIn;
  },
});
