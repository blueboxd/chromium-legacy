// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'personalization-options' contains several toggles related to
 * personalizations.
 */
(function() {

Polymer({
  is: 'settings-personalization-options',

  behaviors: [
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!PrivacyPageVisibility}
     */
    pageVisibility: Object,

    /** @type {settings.SyncStatus} */
    syncStatus: Object,

    /** @private */
    passwordsLeakDetectionEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('passwordsLeakDetectionEnabled');
      },
    },

    // <if expr="_google_chrome and not chromeos">
    // TODO(dbeam): make a virtual.* pref namespace and set/get this normally
    // (but handled differently in C++).
    /** @private {chrome.settingsPrivate.PrefObject} */
    metricsReportingPref_: {
      type: Object,
      value: function() {
        // TODO(dbeam): this is basically only to appease PrefControlBehavior.
        // Maybe add a no-validate attribute instead? This makes little sense.
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /** @private {chrome.settingsPrivate.PrefObject} */
    safeBrowsingReportingPref_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /** @private */
    showRestart_: Boolean,

    /** @private */
    showRestartToast_: Boolean,
    // </if>

    /** @private */
    showSignoutDialog_: Boolean,

    /** @private */
    privacySettingsRedesignEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('privacySettingsRedesignEnabled');
      },
    },

    /** @private */
    syncFirstSetupInProgress_: {
      type: Boolean,
      value: false,
      computed: 'computeSyncFirstSetupInProgress_(syncStatus)',
    },
  },

  observers: [
    'onSafeBrowsingReportingPrefChange_(prefs.safebrowsing.*)',
  ],

  /**
   * @return {boolean}
   * @private
   */
  computeSyncFirstSetupInProgress_: function() {
    return !!this.syncStatus && !!this.syncStatus.firstSetupInProgress;
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();

    // <if expr="_google_chrome and not chromeos">
    const setMetricsReportingPref = this.setMetricsReportingPref_.bind(this);
    this.addWebUIListener('metrics-reporting-change', setMetricsReportingPref);
    this.browserProxy_.getMetricsReporting().then(setMetricsReportingPref);
    // </if>
  },

  /**
   * @return {boolean}
   * @private
   */
  getDisabledExtendedSafeBrowsing_: function() {
    return !this.getPref('safebrowsing.enabled').value;
  },

  /** @private */
  onSafeBrowsingReportingToggleChange_: function() {
    this.setPrefValue(
        'safebrowsing.scout_reporting_enabled',
        this.$$('#safeBrowsingReportingToggle').checked);
  },

  /** @private */
  onSafeBrowsingReportingPrefChange_: function() {
    if (this.prefs == undefined) {
      return;
    }
    const safeBrowsingScoutPref =
        this.getPref('safebrowsing.scout_reporting_enabled');
    const prefValue = !!this.getPref('safebrowsing.enabled').value &&
        !!safeBrowsingScoutPref.value;
    this.safeBrowsingReportingPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: prefValue,
      enforcement: safeBrowsingScoutPref.enforcement,
      controlledBy: safeBrowsingScoutPref.controlledBy,
    };
  },

  // <if expr="_google_chrome and not chromeos">
  /** @private */
  onMetricsReportingChange_: function() {
    const enabled = this.$.metricsReportingControl.checked;
    this.browserProxy_.setMetricsReportingEnabled(enabled);
  },

  /**
   * @param {!MetricsReporting} metricsReporting
   * @private
   */
  setMetricsReportingPref_: function(metricsReporting) {
    const hadPreviousPref = this.metricsReportingPref_.value !== undefined;
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: metricsReporting.enabled,
    };
    if (metricsReporting.managed) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }

    // Ignore the next change because it will happen when we set the pref.
    this.metricsReportingPref_ = pref;

    // TODO(dbeam): remember whether metrics reporting was enabled when Chrome
    // started.
    if (metricsReporting.managed) {
      this.showRestart_ = false;
    } else if (hadPreviousPref) {
      this.showRestart_ = true;
    }
  },

  // </if>

  // <if expr="_google_chrome">
  /**
   * @param {!Event} event
   * @private
   */
  onUseSpellingServiceToggle_: function(event) {
    // If turning on using the spelling service, automatically turn on
    // spellcheck so that the spelling service can run.
    if (event.target.checked) {
      this.setPrefValue('browser.enable_spellchecking', true);
    }
  },
  // </if>

  /**
   * @return {boolean}
   * @private
   */
  showSpellCheckControl_: function() {
    return (
        !!this.prefs.spellcheck &&
        /** @type {!Array<string>} */
        (this.prefs.spellcheck.dictionaries.value).length > 0);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowDriveSuggest_: function() {
    return loadTimeData.getBoolean('driveSuggestAvailable') &&
        !!this.syncStatus && !!this.syncStatus.signedIn &&
        this.syncStatus.statusAction !== settings.StatusAction.REAUTHENTICATE;
  },

  /** @private */
  onSigninAllowedChange_: function() {
    if (this.syncStatus.signedIn && !this.$$('#signinAllowedToggle').checked) {
      // Switch the toggle back on and show the signout dialog.
      this.$$('#signinAllowedToggle').checked = true;
      this.showSignoutDialog_ = true;
    } else {
      /** @type {!SettingsToggleButtonElement} */ (
          this.$$('#signinAllowedToggle'))
          .sendPrefChange();
      this.showRestartToast_ = true;
    }

    this.browserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_CHROME_SIGN_IN);
  },

  /** @private */
  onSignoutDialogClosed_: function() {
    if (/** @type {!SettingsSignoutDialogElement} */ (
            this.$$('settings-signout-dialog'))
            .wasConfirmed()) {
      this.$$('#signinAllowedToggle').checked = false;
      /** @type {!SettingsToggleButtonElement} */ (
          this.$$('#signinAllowedToggle'))
          .sendPrefChange();
      this.showRestartToast_ = true;
    }
    this.showSignoutDialog_ = false;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRestartTap_: function(e) {
    e.stopPropagation();
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
  },
});
})();
