// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 * Names of the radio buttons which allow the user to choose their encryption
 * mechanism.
 * @enum {string}
 */
const RadioButtonNames = {
  ENCRYPT_WITH_GOOGLE: 'encrypt-with-google',
  ENCRYPT_WITH_PASSPHRASE: 'encrypt-with-passphrase',
};

/**
 * All possible states for the sWAA bit.
 * @enum {string}
 */
const sWAAState = {
  NOT_FETCHED: 'not-fetched',
  FETCHING: 'fetching',
  FAILED: 'failed',
  ON: 'On',
  OFF: 'Off',
};

/**
 * @fileoverview
 * 'settings-sync-page' is the settings page containing sync settings.
 */
Polymer({
  is: 'settings-sync-page',

  behaviors: [
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    pages_: {
      type: Object,
      value: settings.PageStatus,
      readOnly: true,
    },

    /**
     * The current page status. Defaults to |CONFIGURE| such that the searching
     * algorithm can search useful content when the page is not visible to the
     * user.
     * @private {?settings.PageStatus}
     */
    pageStatus_: {
      type: String,
      value: settings.PageStatus.CONFIGURE,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!PrivacyPageVisibility}
     */
    pageVisibility: Object,

    /**
     * The current sync preferences, supplied by SyncBrowserProxy.
     * @type {settings.SyncPrefs|undefined}
     */
    syncPrefs: {
      type: Object,
    },

    /** @type {settings.SyncStatus} */
    syncStatus: {
      type: Object,
    },

    /**
     * Whether the "create passphrase" inputs should be shown. These inputs
     * give the user the opportunity to use a custom passphrase instead of
     * authenticating with their Google credentials.
     * @private
     */
    creatingNewPassphrase_: {
      type: Boolean,
      value: false,
    },

    /**
     * The passphrase input field value.
     * @private
     */
    passphrase_: {
      type: String,
      value: '',
    },

    /**
     * The passphrase confirmation input field value.
     * @private
     */
    confirmation_: {
      type: String,
      value: '',
    },

    /**
     * The existing passphrase input field value.
     * @private
     */
    existingPassphrase_: {
      type: String,
      value: '',
    },

    /** @private */
    signedIn_: {
      type: Boolean,
      value: true,
      computed: 'computeSignedIn_(syncStatus.signedIn)',
    },

    /** @private */
    syncDisabledByAdmin_: {
      type: Boolean,
      value: false,
      computed: 'computeSyncDisabledByAdmin_(syncStatus.managed)',
    },

    /** @private */
    syncSectionDisabled_: {
      type: Boolean,
      value: false,
      computed: 'computeSyncSectionDisabled_(' +
          'syncStatus.signedIn, syncStatus.disabled, ' +
          'syncStatus.hasError, syncStatus.statusAction, ' +
          'syncPrefs.trustedVaultKeysRequired)',
    },

    /** @private */
    showSetupCancelDialog_: {
      type: Boolean,
      value: false,
    },

    disableEncryptionOptions_: {
      type: Boolean,
      computed: 'computeDisableEncryptionOptions_(' +
          'syncPrefs, syncStatus)',
    },

    /**
     * If sync page friendly settings is enabled.
     * @private
     */
    syncSetupFriendlySettings_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('syncSetupFriendlySettings') &&
            loadTimeData.getBoolean('syncSetupFriendlySettings');
      }
    },

    /**
     * Whether history is not synced or data is encrypted.
     * @private
     */
    historyNotSyncedOrEncrypted_: {
      type: Boolean,
      computed: 'computeHistoryNotSyncedOrEncrypted_(' +
          'syncPrefs.encryptAllData, syncPrefs.typedUrlsSynced)',
    },

    /** @private */
    hideActivityControlsUrl_: {
      type: Boolean,
      computed: 'computeHideActivityControlsUrl_(historyNotSyncedOrEncrypted_)',
    },

    /** @private */
    sWAA_: {
      type: String,
      value: sWAAState.NOT_FETCHED,
    },
  },

  observers: ['fetchSWAA_(syncSectionDisabled_, historyNotSyncedOrEncrypted_)'],

  /** @private {?settings.SyncBrowserProxy} */
  browserProxy_: null,

  /**
   * The visibility changed callback is used to refetch the |sWAA_| bit when
   * the page is foregrounded.
   * @private {?Function}
   */
  visibilityChangedCallback_: null,

  /**
   * The beforeunload callback is used to show the 'Leave site' dialog. This
   * makes sure that the user has the chance to go back and confirm the sync
   * opt-in before leaving.
   *
   * This property is non-null if the user is currently navigated on the sync
   * settings route.
   *
   * @private {?Function}
   */
  beforeunloadCallback_: null,

  /**
   * The unload callback is used to cancel the sync setup when the user hits
   * the browser back button after arriving on the page.
   * Note: Cases like closing the tab or reloading don't need to be handled,
   * because they are already caught in |PeopleHandler::~PeopleHandler|
   * from the C++ code.
   *
   * @private {?Function}
   */
  unloadCallback_: null,

  /**
   * Whether the initial layout for collapsible sections has been computed. It
   * is computed only once, the first time the sync status is updated.
   * @private {boolean}
   */
  collapsibleSectionsInitialized_: false,

  /**
   * Whether the user decided to abort sync.
   * @private {boolean}
   */
  didAbort_: true,

  /**
   * Whether the user confirmed the cancellation of sync.
   * @private {boolean}
   */
  setupCancelConfirmed_: false,

  /** @override */
  created() {
    this.browserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'page-status-changed', this.handlePageStatusChanged_.bind(this));
    this.addWebUIListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    const router = settings.Router.getInstance();
    if (router.getCurrentRoute() == router.getRoutes().SYNC) {
      this.onNavigateToPage_();
    }
  },

  /** @override */
  detached() {
    const router = settings.Router.getInstance();
    if (router.getRoutes().SYNC.contains(router.getCurrentRoute())) {
      this.onNavigateAwayFromPage_();
    }

    if (this.beforeunloadCallback_) {
      window.removeEventListener('beforeunload', this.beforeunloadCallback_);
      this.beforeunloadCallback_ = null;
    }
    if (this.unloadCallback_) {
      window.removeEventListener('unload', this.unloadCallback_);
      this.unloadCallback_ = null;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSignedIn_() {
    return !!this.syncStatus.signedIn;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSyncSectionDisabled_() {
    return this.syncStatus !== undefined &&
        (!this.syncStatus.signedIn || !!this.syncStatus.disabled ||
         (!!this.syncStatus.hasError &&
          this.syncStatus.statusAction !==
              settings.StatusAction.ENTER_PASSPHRASE &&
          this.syncStatus.statusAction !==
              settings.StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS));
  },

  /**
   * @return {boolean} Returns true if History sync is off or data is encrypted.
   * @private
   */
  computeHistoryNotSyncedOrEncrypted_() {
    return !!(this.syncPrefs) &&
        (!this.syncPrefs.typedUrlsSynced || this.syncPrefs.encryptAllData);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHideActivityControlsUrl_() {
    return !!this.syncSetupFriendlySettings_ &&
        !!this.historyNotSyncedOrEncrypted_;
  },


  /**
   * Compute and fetch the sWAA bit for sync users. sWAA is 'OFF' if sync
   * history is off or data is encrypted with custom passphrase. Otherwise,
   * a query to |Web and App Activity| is needed.
   * @private
   */
  fetchSWAA_() {
    const router = settings.Router.getInstance();
    if (router.getCurrentRoute() !== router.getRoutes().SYNC) {
      return;
    }

    if (!this.syncSetupFriendlySettings_) {
      return;
    }

    if (!this.syncPrefs || this.syncPrefs.encryptAllData === undefined ||
        this.syncPrefs.typedUrlsSynced === undefined) {
      return;
    }

    if (this.syncSectionDisabled_) {
      this.sWAA_ = sWAAState.NOT_FETCHED;
      return;
    }

    if (this.historyNotSyncedOrEncrypted_) {
      this.sWAA_ = sWAAState.OFF;
      return;
    }

    if (this.sWAA_ === sWAAState.FETCHING) {
      return;
    }

    this.sWAA_ = sWAAState.FETCHING;
    const updateSWAA = historyRecordingEnabled => {
      this.sWAA_ = historyRecordingEnabled.requestSucceeded ?
          (historyRecordingEnabled.historyRecordingEnabled ? sWAAState.ON :
                                                             sWAAState.OFF) :
          sWAAState.FAILED;
    };
    this.browserProxy_.queryIsHistoryRecordingEnabled().then(updateSWAA);
  },

  /**
   * Refetch sWAA when the page is forgrounded, to guarantee the value shown is
   * most up-to-date.
   * @private
   */
  visibilityHandler_() {
    if (document.visibilityState === 'visible') {
      this.fetchSWAA_();
    }
  },

  /**
   * Return hint to explain the sWAA state. It is displayed as secondary text in
   * the history usage row.
   * @private
   */
  getHistoryUsageHint_() {
    if (this.sWAA_ === sWAAState.ON) {
      return this.i18n('sWAAOnHint');
    }

    if (this.sWAA_ === sWAAState.OFF) {
      if (this.syncPrefs.encryptAllData) {
        return this.i18n('dataEncryptedHint');
      }

      if (!this.syncPrefs.typedUrlsSynced) {
        return this.i18n('historySyncOffHint');
      }
      return this.i18n('sWAAOffHint');
    }
    return '';
  },

  /**
   * @private
   */
  getSWAAStateText_() {
    if (!this.isSWAAFetched_()) {
      return '';
    }
    return this.i18n(this.sWAA_ === sWAAState.ON ? 'sWAAOn' : 'sWAAOff');
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSyncDisabledByAdmin_() {
    return this.syncStatus != undefined && !!this.syncStatus.managed;
  },

  /** @private */
  onSetupCancelDialogBack_() {
    this.$$('#setupCancelDialog').cancel();
    chrome.metricsPrivate.recordUserAction(
        'Signin_Signin_CancelCancelAdvancedSyncSettings');
  },

  /** @private */
  onSetupCancelDialogConfirm_() {
    this.setupCancelConfirmed_ = true;
    this.$$('#setupCancelDialog').close();
    const router = settings.Router.getInstance();
    router.navigateTo(
        /** @type {!settings.Route} */ (router.getRoutes().BASIC));
    chrome.metricsPrivate.recordUserAction(
        'Signin_Signin_ConfirmCancelAdvancedSyncSettings');
  },

  /** @private */
  onSetupCancelDialogClose_() {
    this.showSetupCancelDialog_ = false;
  },

  /** @protected */
  currentRouteChanged() {
    const router = settings.Router.getInstance();
    if (router.getCurrentRoute() == router.getRoutes().SYNC) {
      this.onNavigateToPage_();
      return;
    }

    if (router.getRoutes().SYNC.contains(router.getCurrentRoute())) {
      return;
    }

    const searchParams =
        settings.Router.getInstance().getQueryParameters().get('search');
    if (searchParams) {
      // User navigated away via searching. Cancel sync without showing
      // confirmation dialog.
      this.onNavigateAwayFromPage_();
      return;
    }

    const userActionCancelsSetup = this.syncStatus &&
        this.syncStatus.firstSetupInProgress && this.didAbort_;
    if (userActionCancelsSetup && !this.setupCancelConfirmed_) {
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_BackOnAdvancedSyncSettings');
      // Show the 'Cancel sync?' dialog.
      // Yield so that other |currentRouteChanged| observers are called,
      // before triggering another navigation (and another round of observers
      // firing). Triggering navigation from within an observer leads to some
      // undefined behavior and runtime errors.
      requestAnimationFrame(() => {
        router.navigateTo(
            /** @type {!settings.Route} */ (router.getRoutes().SYNC));
        this.showSetupCancelDialog_ = true;
        // Flush to make sure that the setup cancel dialog is attached.
        Polymer.dom.flush();
        this.$$('#setupCancelDialog').showModal();
      });
      return;
    }

    // Reset variable.
    this.setupCancelConfirmed_ = false;

    this.onNavigateAwayFromPage_();
  },

  /**
   * @param {!settings.PageStatus} expectedPageStatus
   * @return {boolean}
   * @private
   */
  isStatus_(expectedPageStatus) {
    return expectedPageStatus == this.pageStatus_;
  },

  /**
   * @return {boolean}
   * @private
   */
  isSWAAFetching_() {
    return this.sWAA_ === sWAAState.FETCHING;
  },

  /**
   * @return {boolean}
   * @private
   */
  isSWAAFetched_() {
    return this.sWAA_ === sWAAState.ON || this.sWAA_ === sWAAState.OFF;
  },

  /** @private */
  onNavigateToPage_() {
    const router = settings.Router.getInstance();
    assert(router.getCurrentRoute() == router.getRoutes().SYNC);
    this.sWAA_ = sWAAState.NOT_FETCHED;
    this.fetchSWAA_();
    if (this.beforeunloadCallback_) {
      return;
    }

    this.collapsibleSectionsInitialized_ = false;

    // Display loading page until the settings have been retrieved.
    this.pageStatus_ = settings.PageStatus.SPINNER;

    this.browserProxy_.didNavigateToSyncPage();

    this.beforeunloadCallback_ = event => {
      // When the user tries to leave the sync setup, show the 'Leave site'
      // dialog.
      if (this.syncStatus && this.syncStatus.firstSetupInProgress) {
        event.preventDefault();
        event.returnValue = '';

        chrome.metricsPrivate.recordUserAction(
            'Signin_Signin_AbortAdvancedSyncSettings');
      }
    };
    window.addEventListener('beforeunload', this.beforeunloadCallback_);

    this.unloadCallback_ = this.onNavigateAwayFromPage_.bind(this);
    window.addEventListener('unload', this.unloadCallback_);

    this.visibilityChangedCallback_ = this.visibilityHandler_.bind(this);
    window.addEventListener('focus', this.visibilityChangedCallback_);
    document.addEventListener(
        'visibilitychange', this.visibilityChangedCallback_);
  },

  /** @private */
  onNavigateAwayFromPage_() {
    if (!this.beforeunloadCallback_) {
      return;
    }

    // Reset the status to CONFIGURE such that the searching algorithm can
    // search useful content when the page is not visible to the user.
    this.pageStatus_ = settings.PageStatus.CONFIGURE;

    this.browserProxy_.didNavigateAwayFromSyncPage(this.didAbort_);

    window.removeEventListener('beforeunload', this.beforeunloadCallback_);
    this.beforeunloadCallback_ = null;

    if (this.unloadCallback_) {
      window.removeEventListener('unload', this.unloadCallback_);
      this.unloadCallback_ = null;
    }

    if (this.visibilityChangedCallback_) {
      window.removeEventListener('focus', this.visibilityChangedCallback_);
      document.removeEventListener(
          'visibilitychange', this.visibilityChangedCallback_);
      this.visibilityChangedCallback_ = null;
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleSyncPrefsChanged_(syncPrefs) {
    this.syncPrefs = syncPrefs;
    this.pageStatus_ = settings.PageStatus.CONFIGURE;

    // Hide the new passphrase box if (a) full data encryption is enabled,
    // (b) encrypting all data is not allowed (so far, only applies to
    // supervised accounts), or (c) the user is a supervised account.
    if (this.syncPrefs.encryptAllData ||
        !this.syncPrefs.encryptAllDataAllowed ||
        (this.syncStatus && this.syncStatus.supervisedUser)) {
      this.creatingNewPassphrase_ = false;
    }

    if (this.sWAA_ === sWAAState.FAILED) {
      this.fetchSWAA_();
    }
  },

  /** @private */
  onActivityControlsTap_() {
    this.browserProxy_.openActivityControlsUrl();
  },

  /**
   * @param {string} passphrase The passphrase input field value
   * @param {string} confirmation The passphrase confirmation input field value.
   * @return {boolean} Whether the passphrase save button should be enabled.
   * @private
   */
  isSaveNewPassphraseEnabled_(passphrase, confirmation) {
    return passphrase !== '' && confirmation !== '';
  },

  /**
   * Sends the newly created custom sync passphrase to the browser.
   * @private
   * @param {!Event} e
   */
  onSaveNewPassphraseTap_(e) {
    assert(this.creatingNewPassphrase_);

    // Ignore events on irrelevant elements or with irrelevant keys.
    if (e.target.tagName != 'CR-BUTTON' && e.target.tagName != 'CR-INPUT') {
      return;
    }
    if (e.type == 'keypress' && e.key != 'Enter') {
      return;
    }

    // If a new password has been entered but it is invalid, do not send the
    // sync state to the API.
    if (!this.validateCreatedPassphrases_()) {
      return;
    }

    this.syncPrefs.encryptAllData = true;
    this.syncPrefs.setNewPassphrase = true;
    this.syncPrefs.passphrase = this.passphrase_;

    this.browserProxy_.setSyncEncryption(this.syncPrefs)
        .then(this.handlePageStatusChanged_.bind(this));
  },

  /**
   * Sends the user-entered existing password to re-enable sync.
   * @private
   * @param {!Event} e
   */
  onSubmitExistingPassphraseTap_(e) {
    if (e.type == 'keypress' && e.key != 'Enter') {
      return;
    }

    assert(!this.creatingNewPassphrase_);

    this.syncPrefs.setNewPassphrase = false;

    this.syncPrefs.passphrase = this.existingPassphrase_;
    this.existingPassphrase_ = '';

    this.browserProxy_.setSyncEncryption(this.syncPrefs)
        .then(this.handlePageStatusChanged_.bind(this));
  },

  /**
   * Called when the page status updates.
   * @param {!settings.PageStatus} pageStatus
   * @private
   */
  handlePageStatusChanged_(pageStatus) {
    const router = settings.Router.getInstance();
    switch (pageStatus) {
      case settings.PageStatus.SPINNER:
      case settings.PageStatus.TIMEOUT:
      case settings.PageStatus.CONFIGURE:
        this.pageStatus_ = pageStatus;
        return;
      case settings.PageStatus.DONE:
        if (router.getCurrentRoute() == router.getRoutes().SYNC) {
          router.navigateTo(
              /** @type {!settings.Route} */ (router.getRoutes().PEOPLE));
        }
        return;
      case settings.PageStatus.PASSPHRASE_FAILED:
        if (this.pageStatus_ == this.pages_.CONFIGURE && this.syncPrefs &&
            this.syncPrefs.passphraseRequired) {
          const passphraseInput = /** @type {!CrInputElement} */ (
              this.$$('#existingPassphraseInput'));
          passphraseInput.invalid = true;
          passphraseInput.focusInput();
        }
        return;
    }

    assertNotReached();
  },

  /**
   * Called when the encryption
   * @param {!Event} event
   * @private
   */
  onEncryptionRadioSelectionChanged_(event) {
    this.creatingNewPassphrase_ =
        event.detail.value == RadioButtonNames.ENCRYPT_WITH_PASSPHRASE;
  },

  /**
   * Computed binding returning the selected encryption radio button.
   * @private
   */
  selectedEncryptionRadio_() {
    return this.syncPrefs.encryptAllData || this.creatingNewPassphrase_ ?
        RadioButtonNames.ENCRYPT_WITH_PASSPHRASE :
        RadioButtonNames.ENCRYPT_WITH_GOOGLE;
  },

  /**
   * Checks the supplied passphrases to ensure that they are not empty and that
   * they match each other. Additionally, displays error UI if they are invalid.
   * @return {boolean} Whether the check was successful (i.e., that the
   *     passphrases were valid).
   * @private
   */
  validateCreatedPassphrases_() {
    const emptyPassphrase = !this.passphrase_;
    const mismatchedPassphrase = this.passphrase_ != this.confirmation_;

    this.$$('#passphraseInput').invalid = emptyPassphrase;
    this.$$('#passphraseConfirmationInput').invalid =
        !emptyPassphrase && mismatchedPassphrase;

    return !emptyPassphrase && !mismatchedPassphrase;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreTap_(event) {
    if (event.target.tagName == 'A') {
      // Stop the propagation of events, so that clicking on links inside
      // checkboxes or radio buttons won't change the value.
      event.stopPropagation();
    }
  },

  /**
   * When there is a sync passphrase, some items have an additional line for the
   * passphrase reset hint, making them three lines rather than two.
   * @return {string}
   * @private
   */
  getPassphraseHintLines_() {
    return this.syncPrefs.encryptAllData ? 'three-line' : 'two-line';
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSyncAccountControl_() {
    // <if expr="chromeos">
    if (!loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      return false;
    }
    // </if>
    return this.syncStatus !== undefined &&
        !!this.syncStatus.syncSystemEnabled && !!this.syncStatus.signinAllowed;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowExistingPassphraseBelowAccount_() {
    return this.syncPrefs !== undefined && !!this.syncPrefs.passphraseRequired;
  },

  /**
   * Whether we should disable the radio buttons that allow choosing the
   * encryption options for Sync.
   * We disable the buttons if:
   * (a) full data encryption is enabled, or,
   * (b) full data encryption is not allowed (so far, only applies to
   * supervised accounts), or,
   * (c) current encryption keys are missing, or,
   * (d) the user is a supervised account.
   * @return {boolean}
   * @private
   */
  computeDisableEncryptionOptions_() {
    return !!(
        (this.syncPrefs &&
         (this.syncPrefs.encryptAllData ||
          !this.syncPrefs.encryptAllDataAllowed ||
          this.syncPrefs.trustedVaultKeysRequired)) ||
        (this.syncStatus && this.syncStatus.supervisedUser));
  },

  /** @private */
  onSyncAdvancedTap_() {
    const router = settings.Router.getInstance();
    router.navigateTo(
        /** @type {!settings.Route} */ (router.getRoutes().SYNC_ADVANCED));
  },

  /**
   * @param {!CustomEvent<boolean>} e The event passed from
   *     settings-sync-account-control.
   * @private
   */
  onSyncSetupDone_(e) {
    if (e.detail) {
      this.didAbort_ = false;
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_ConfirmAdvancedSyncSettings');
    } else {
      this.setupCancelConfirmed_ = true;
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_CancelAdvancedSyncSettings');
    }
    const router = settings.Router.getInstance();
    router.navigateTo(
        /** @type {!settings.Route} */ (router.getRoutes().BASIC));
  },

  /**
   * Focuses the passphrase input element if it is available and the page is
   * visible.
   * @private
   */
  focusPassphraseInput_() {
    const passphraseInput =
        /** @type {!CrInputElement} */ (this.$$('#existingPassphraseInput'));
    const router = settings.Router.getInstance();
    if (passphraseInput &&
        router.getCurrentRoute() === router.getRoutes().SYNC) {
      passphraseInput.focus();
    }
  },
});

})();
