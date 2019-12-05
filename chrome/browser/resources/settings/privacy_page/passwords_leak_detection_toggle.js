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
  },

  /** @override */
  ready: function() {
    // <if expr="not chromeos">
    const storedAccountsChanged = storedAccounts => this.storedAccounts_ =
        storedAccounts;
    const syncBrowserProxy = settings.SyncBrowserProxyImpl.getInstance();
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>

    // Even though we already set checked="[[getCheckedLeakDetection_(...)]]"
    // in the DOM, this might be overridden within prefValueChanged_ of
    // SettingsBooleanControlBehaviorImpl which gets invoked once we navigate to
    // sync_page.html. Re-computing the checked value here once fixes this
    // problem.
    this.$.passwordsLeakDetectionCheckbox.checked =
        this.getCheckedLeakDetection_();
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
   * @return {boolean}
   * @private
   */
  getCheckedLeakDetection_: function() {
    return this.userSignedIn_ && this.passwordsLeakDetectionAvailable_;
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
});
