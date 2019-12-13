// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-passwords-leak-detection-toggle',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {settings.SyncStatus} */
    syncStatus: Object,

    // <if expr="not chromeos">
    /** @private {Array<!settings.StoredAccount>} */
    storedAccounts_: Object,
    // </if>

    /** @private */
    userSignedIn_: {
      type: Boolean,
      computed: 'computeUserSignedIn_(syncStatus, storedAccounts_)',
    },

    /** @private */
    passwordsLeakDetectionAvailable_: {
      type: Boolean,
      computed: 'computePasswordsLeakDetectionAvailable_(prefs.*)',
    },

    // A "virtual" preference that is use to control the associated toggle
    // independently of the preference it represents. This is updated with
    // the registered setPasswordsLeakDetectionPref_ observer. Changes to
    // the real preference are handled in onPasswordsLeakDetectionChange_
    /** @private {chrome.settingsPrivate.PrefObject} */
    passwordsLeakDetectionPref_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },
  },

  observers: [
    'setPasswordsLeakDetectionPref_(userSignedIn_,' +
        'passwordsLeakDetectionAvailable_)',
  ],

  /** @override */
  ready: function() {
    // <if expr="not chromeos">
    const storedAccountsChanged = storedAccounts => this.storedAccounts_ =
        storedAccounts;
    const syncBrowserProxy = settings.SyncBrowserProxyImpl.getInstance();
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>
  },

  /**
   * @return {boolean}
   * @private
   */
  computeUserSignedIn_: function() {
    return (!!this.syncStatus && !!this.syncStatus.signedIn) ?
        !this.syncStatus.hasError :
        (!!this.storedAccounts_ && this.storedAccounts_.length > 0);
  },

  /**
   * @return {boolean}
   * @private
   */
  computePasswordsLeakDetectionAvailable_: function() {
    return !!this.getPref('profile.password_manager_leak_detection').value &&
        !!this.getPref('safebrowsing.enabled').value;
  },

  /**
   * @return {string}
   * @private
   */
  getPasswordsLeakDetectionSubLabel_: function() {
    if (!this.userSignedIn_ && this.passwordsLeakDetectionAvailable_) {
      return this.i18n('passwordsLeakDetectionSignedOutEnabledDescription');
    }
    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  getDisabledLeakDetection_: function() {
    return !this.userSignedIn_ || !this.getPref('safebrowsing.enabled').value;
  },

  /** @private */
  onPasswordsLeakDetectionChange_: function() {
    this.setPrefValue(
        'profile.password_manager_leak_detection',
        this.$.passwordsLeakDetectionCheckbox.checked);
  },

  /** @private */
  setPasswordsLeakDetectionPref_: function() {
    if (this.prefs == undefined) {
      return;
    }
    const passwordManagerLeakDetectionPref =
        this.getPref('profile.password_manager_leak_detection');
    const prefValue =
        this.userSignedIn_ && this.passwordsLeakDetectionAvailable_;
    this.passwordsLeakDetectionPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: prefValue,
      enforcement: passwordManagerLeakDetectionPref.enforcement,
      controlledBy: passwordManagerLeakDetectionPref.controlledBy,
    };
  },
});
