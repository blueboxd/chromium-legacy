// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './collapse_radio_button.js';
import './disable_safebrowsing_dialog.js';
import './passwords_leak_detection_toggle.js';
import './secure_dns.js';
import '../controls/settings_toggle_button.m.js';
import '../icons.m.js';
import '../prefs/prefs.m.js';
import '../settings_shared_css.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {SyncStatus} from '../people_page/sync_browser_proxy.m.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

import {PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from './privacy_page_browser_proxy.m.js';
import {SafeBrowsingBrowserProxy, SafeBrowsingBrowserProxyImpl, SafeBrowsingRadioManagedState} from './safe_browsing_browser_proxy.js';

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

  _template: html`{__html_template__}`,

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    /** @type {SyncStatus} */
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

    /** @private */
    safeBrowsingEnhancedEnabled_: {
      type: Boolean,
      readOnly: true,
      value: function() {
        return loadTimeData.getBoolean('safeBrowsingEnhancedEnabled');
      },
    },

    /** @private {!SafeBrowsingRadioManagedState} */
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
    if (routes.CERTIFICATES) {
      this.focusConfig.set(routes.CERTIFICATES.path, () => {
        focusWithoutInk(assert(this.$$('#manageCertificates')));
      });
    }
    // </if>

    if (routes.SECURITY_KEYS) {
      this.focusConfig.set(routes.SECURITY_KEYS.path, () => {
        focusWithoutInk(assert(this.$$('#security-keys-subpage-trigger')));
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

  /** @private {PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @private {MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = PrivacyPageBrowserProxyImpl.getInstance();

    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    SafeBrowsingBrowserProxyImpl.getInstance().validateSafeBrowsingEnhanced();
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
        await SafeBrowsingBrowserProxyImpl.getInstance()
            .getSafeBrowsingRadioManagedState();
  },

  /** @private */
  onManageCertificatesClick_() {
    // <if expr="use_nss_certs">
    Router.getInstance().navigateTo(routes.CERTIFICATES);
    // </if>
    // <if expr="is_win or is_macosx">
    this.browserProxy_.showManageSSLCertificates();
    // </if>
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.MANAGE_CERTIFICATES);
  },

  /** @private */
  onAdvancedProtectionProgramLinkClick_() {
    window.open(loadTimeData.getString('advancedProtectionURL'));
  },

  /** @private */
  onSecurityKeysClick_() {
    Router.getInstance().navigateTo(routes.SECURITY_KEYS);
  },

  /** @private */
  onSafeBrowsingExtendedReportingChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.IMPROVE_SECURITY);
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
    focusWithoutInk(assert(this.$.safeBrowsingDisabled));
  },
});
