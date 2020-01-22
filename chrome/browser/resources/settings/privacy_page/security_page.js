// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-security-page',

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    /** @type {settings.SyncStatus} */
    syncStatus: Object,

    /** @private */
    enableSecurityKeysSubpage_: {
      type: Boolean,
      readOnly: true,
      value() {
        return loadTimeData.getBoolean('enableSecurityKeysSubpage');
      }
    },

    /** @private {chrome.settingsPrivate.PrefObject} */
    safeBrowsingReportingPref_: {
      type: Object,
      value() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({
          key: '',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        });
      },
    },
  },

  observers: [
    'onSafeBrowsingReportingPrefChange_(prefs.safebrowsing.*)',
  ],

  /** @private {settings.PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();

    this.metricsBrowserProxy_ = settings.MetricsBrowserProxyImpl.getInstance();
  },

  /**
   * @return {boolean}
   * @private
   */
  getDisabledExtendedSafeBrowsing_() {
    return !this.getPref('safebrowsing.enabled').value;
  },

  /** @private */
  onSafeBrowsingReportingToggleChange_() {
    this.setPrefValue(
        'safebrowsing.scout_reporting_enabled',
        this.$$('#safeBrowsingReportingToggle').checked);
  },

  /** @private */
  onSafeBrowsingReportingPrefChange_() {
    if (this.prefs === undefined) {
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

  /** @private */
  onManageCertificatesClick_() {
    // <if expr="use_nss_certs">
    settings.Router.getInstance().navigateTo(settings.routes.CERTIFICATES);
    // </if>
    // <if expr="is_win or is_macosx">
    this.browserProxy_.showManageSSLCertificates();
    // </if>
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_MANAGE_CERTIFICATES);
  },

  /** @private */
  onAdvancedProtectionProgramLinkClick_() {
    window.open('https://landing.google.com/advancedprotection/');
  },

  /** @private */
  onSecurityKeysClick_() {
    settings.Router.getInstance().navigateTo(settings.routes.SECURITY_KEYS);
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_SECURITY_KEYS);
  },
});
