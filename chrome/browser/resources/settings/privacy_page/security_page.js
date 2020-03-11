// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
/**
 * Enumeration of all safe browsing modes.
 * @enum {string}
 */
const SafeBrowsing = {
  ENHANCED: 'enhanced',
  STANDARD: 'standard',
  DISABLED: 'disabled',
};

Polymer({
  is: 'settings-security-page',

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    /** @type {settings.SyncStatus} */
    syncStatus: Object,

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether the secure DNS setting should be displayed.
     * @private
     */
    showSecureDnsSetting_: {
      type: Boolean,
      readOnly: true,
      value: function() {
        return loadTimeData.getBoolean('showSecureDnsSetting');
      },
    },

    /**
     * Valid safe browsing states.
     * @private
     */
    safeBrowsingEnum_: {
      type: Object,
      value: SafeBrowsing,
    },

    /** @private */
    selectSafeBrowsingRadio_: {
      type: String,
      computed: 'computeSelectSafeBrowsingRadio_(prefs.safeBrowsing.*)',
    },

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

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        // <if expr="use_nss_certs">
        if (settings.routes.CERTIFICATES) {
          map.set(settings.routes.CERTIFICATES.path, '#manageCertificates');
        }
        // </if>

        if (settings.routes.SECURITY_KEYS) {
          map.set(
              settings.routes.SECURITY_KEYS.path,
              '#security-keys-subpage-trigger');
        }
        return map;
      },
    },
  },

  observers: [
    'onSafeBrowsingReportingPrefChange_(prefs.safebrowsing.*)',
  ],

  /**
   * @return {string}
   * @private
   */
  computeSelectSafeBrowsingRadio_() {
    if (this.prefs === undefined) {
      return SafeBrowsing.STANDARD;
    }
    if (!this.getPref('safebrowsing.enabled').value) {
      return SafeBrowsing.DISABLED;
    }
    return this.getPref('safebrowsing.enhanced').value ? SafeBrowsing.ENHANCED :
                                                         SafeBrowsing.STANDARD;
  },


  /** @private {settings.PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @private {settings.MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();

    this.metricsBrowserProxy_ = settings.MetricsBrowserProxyImpl.getInstance();
  },

  /**
   * Updates the various underlying cookie prefs based on the newly selected
   * radio button.
   * @param {!CustomEvent<{value: string}>} event
   * @private
   */
  onSafeBrowsingRadioChange_: function(event) {
    if (event.detail.value == SafeBrowsing.ENHANCED) {
      this.setPrefValue('safebrowsing.enabled', true);
      this.setPrefValue('safebrowsing.enhanced', true);
    } else if (event.detail.value == SafeBrowsing.STANDARD) {
      this.setPrefValue('safebrowsing.enabled', true);
      this.setPrefValue('safebrowsing.enhanced', false);
    } else {  // disabled state
      this.setPrefValue('safebrowsing.enabled', false);
      this.setPrefValue('safebrowsing.enhanced', false);
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  getDisabledExtendedSafeBrowsing_() {
    return !this.getPref('safebrowsing.enabled').value ||
        !!this.getPref('safebrowsing.enhanced').value;
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
    this.safeBrowsingReportingPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: !!safeBrowsingScoutPref.value,
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
    window.open(loadTimeData.getString('advancedProtectionURL'));
  },

  /** @private */
  onSecurityKeysClick_() {
    settings.Router.getInstance().navigateTo(settings.routes.SECURITY_KEYS);
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        settings.SettingsPageInteractions.PRIVACY_SECURITY_KEYS);
  },
});
})();
