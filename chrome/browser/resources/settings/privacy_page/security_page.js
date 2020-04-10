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
      computed: 'computeSelectSafeBrowsingRadio_(prefs.safebrowsing.*)',
    },

    /** @private {!settings.SafeBrowsingRadioManagedState} */
    safeBrowsingRadioManagedState_: Object,

    /** @private */
    enableSecurityKeysSubpage_: {
      type: Boolean,
      readOnly: true,
      value() {
        return loadTimeData.getBoolean('enableSecurityKeysSubpage');
      }
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },

    /** @private */
    showDisableSafebrowsingDialog_: Boolean,
  },

  observers: [
    'onSafeBrowsingPrefChange_(prefs.safebrowsing.*)',
  ],

  /*
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    assert(!oldConfig);
    // <if expr="use_nss_certs">
    if (settings.routes.CERTIFICATES) {
      this.focusConfig.set(settings.routes.CERTIFICATES.path, () => {
        cr.ui.focusWithoutInk(assert(this.$$('#manageCertificates')));
      });
    }
    // </if>

    if (settings.routes.SECURITY_KEYS) {
      this.focusConfig.set(settings.routes.SECURITY_KEYS.path, () => {
        cr.ui.focusWithoutInk(
            assert(this.$$('#security-keys-subpage-trigger')));
      });
    }
  },

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
      this.showDisableSafebrowsingDialog_ = true;
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
  async onSafeBrowsingPrefChange_() {
    // Retrieve and update safe browsing radio managed state.
    this.safeBrowsingRadioManagedState_ =
        await settings.SafeBrowsingBrowserProxyImpl.getInstance()
            .getSafeBrowsingRadioManagedState();
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
        settings.PrivacyElementInteractions.MANAGE_CERTIFICATES);
  },

  /** @private */
  onAdvancedProtectionProgramLinkClick_() {
    window.open(loadTimeData.getString('advancedProtectionURL'));
  },

  /** @private */
  onSecurityKeysClick_() {
    settings.Router.getInstance().navigateTo(settings.routes.SECURITY_KEYS);
  },

  /**
   * Handles the closure of the disable safebrowsing dialog, reselects the
   * appropriate radio button if the user cancels the dialog, and puts focus on
   * the disable safebrowsing button.
   * @private
   */
  onDisableSafebrowsingDialogClose_() {
    // Check if the dialog was confirmed before closing it.
    if (/** @type {!SettingsDisableSafebrowsingDialogElement} */
        (this.$$('settings-disable-safebrowsing-dialog')).wasConfirmed()) {
      this.setPrefValue('safebrowsing.enabled', false);
      this.setPrefValue('safebrowsing.enhanced', false);
    }

    this.showDisableSafebrowsingDialog_ = false;

    // Have the correct radio button highlighted.
    this.$.safeBrowsingRadio.selected = this.selectSafeBrowsingRadio_;

    // Set focus back to the no protection button regardless of user interaction
    // with the dialog, as it was the entry point to the dialog.
    cr.ui.focusWithoutInk(assert(this.$.safeBrowsingDisabled));
  },
});
})();
