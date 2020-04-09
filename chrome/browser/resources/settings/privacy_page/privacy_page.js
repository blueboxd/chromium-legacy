// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
cr.define('settings', function() {
  /**
   * @typedef {{
   *   enabled: boolean,
   *   pref: !chrome.settingsPrivate.PrefObject
   * }}
   */
  let BlockAutoplayStatus;

  /**
   * Must be kept in sync with the C++ enum of the same name.
   * @enum {number}
   */
  const NetworkPredictionOptions = {
    ALWAYS: 0,
    WIFI_ONLY: 1,
    NEVER: 2,
    DEFAULT: 1,
  };

  Polymer({
    is: 'settings-privacy-page',

    behaviors: [
      PrefsBehavior,
      settings.RouteObserverBehavior,
      I18nBehavior,
      WebUIListenerBehavior,
    ],

    properties: {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       * @type {?settings.SyncStatus}
       */
      syncStatus: Object,

      /**
       * Dictionary defining page visibility.
       * @type {!PrivacyPageVisibility}
       */
      pageVisibility: Object,

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

      /** @private */
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        }
      },

      /** @private */
      showClearBrowsingDataDialog_: Boolean,

      /**
       * Used for HTML bindings. This is defined as a property rather than
       * within the ready callback, because the value needs to be available
       * before local DOM initialization - otherwise, the toggle has unexpected
       * behavior.
       * @private
       */
      networkPredictionUncheckedValue_: {
        type: Number,
        value: NetworkPredictionOptions.NEVER,
      },

      /** @private */
      enableSafeBrowsingSubresourceFilter_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
        }
      },

      /** @private */
      privacySettingsRedesignEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('privacySettingsRedesignEnabled');
        },
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
       * Whether the more settings list is opened.
       * @private
       */
      moreOpened_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      cookieSettingDescription_: String,

      /** @private */
      enableBlockAutoplayContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableBlockAutoplayContentSetting');
        }
      },

      /** @private {settings.BlockAutoplayStatus} */
      blockAutoplayStatus_: {
        type: Object,
        value() {
          return /** @type {settings.BlockAutoplayStatus} */ ({});
        }
      },

      /** @private */
      enablePaymentHandlerContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePaymentHandlerContentSetting');
        }
      },

      /** @private */
      enableExperimentalWebPlatformFeatures_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableExperimentalWebPlatformFeatures');
        },
      },

      /** @private */
      enableSecurityKeysSubpage_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.getBoolean('enableSecurityKeysSubpage');
        }
      },

      /** @private */
      enableInsecureContentContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableInsecureContentContentSetting');
        }
      },

      /** @private */
      enableNativeFileSystemWriteContentSetting_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'enableNativeFileSystemWriteContentSetting');
        }
      },

      /** @private */
      enableQuietNotificationPromptsSetting_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableQuietNotificationPromptsSetting'),
      },

      /** @private */
      enableWebBluetoothNewPermissionsBackend_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
      },

      /** @private */
      enableWebXrContentSetting_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableWebXrContentSetting'),
      },

      /** @private {!Map<string, string>} */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();

          if (this.privacySettingsRedesignEnabled_) {
            if (settings.routes.SECURITY) {
              map.set(settings.routes.SECURITY.path, '#securityLinkRow');
            }

            if (settings.routes.COOKIES) {
              map.set(
                  `${settings.routes.COOKIES.path}_${
                      settings.routes.PRIVACY.path}`,
                  '#cookiesLinkRow');
              map.set(
                  `${settings.routes.COOKIES.path}_${
                      settings.routes.BASIC.path}`,
                  '#cookiesLinkRow');
            }

            if (settings.routes.SITE_SETTINGS) {
              map.set(
                  settings.routes.SITE_SETTINGS.path, '#permissionsLinkRow');
            }
          } else {
            // <if expr="use_nss_certs">
            if (settings.routes.CERTIFICATES) {
              map.set(settings.routes.CERTIFICATES.path, '#manageCertificates');
            }
            // </if>
            if (settings.routes.SITE_SETTINGS) {
              map.set(
                  settings.routes.SITE_SETTINGS.path,
                  '#site-settings-subpage-trigger');
            }

            if (settings.routes.SITE_SETTINGS_SITE_DATA) {
              map.set(
                  settings.routes.SITE_SETTINGS_SITE_DATA.path,
                  '#site-data-trigger');
            }

            if (settings.routes.SECURITY_KEYS) {
              map.set(
                  settings.routes.SECURITY_KEYS.path,
                  '#security-keys-subpage-trigger');
            }
          }

          return map;
        },
      },

      /** @private */
      searchFilter_: String,

      /** @private */
      siteDataFilter_: String,
    },

    observers: [
      'onSafeBrowsingReportingPrefChange_(prefs.safebrowsing.*)',
    ],

    /** @private {?settings.PrivacyPageBrowserProxy} */
    browserProxy_: null,

    /** @override */
    ready() {
      this.ContentSettingsTypes = settings.ContentSettingsTypes;
      this.ChooserType = settings.ChooserType;

      this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();
      this.metricsBrowserProxy_ =
          settings.MetricsBrowserProxyImpl.getInstance();

      this.onBlockAutoplayStatusChanged_({
        pref: /** @type {chrome.settingsPrivate.PrefObject} */ ({value: false}),
        enabled: false
      });

      this.addWebUIListener(
          'onBlockAutoplayStatusChanged',
          this.onBlockAutoplayStatusChanged_.bind(this));

      settings.SyncBrowserProxyImpl.getInstance().getSyncStatus().then(
          this.handleSyncStatus_.bind(this));
      this.addWebUIListener(
          'sync-status-changed', this.handleSyncStatus_.bind(this));

      settings.SiteSettingsPrefsBrowserProxyImpl.getInstance()
          .getCookieSettingDescription()
          .then(description => this.cookieSettingDescription_ = description);
      this.addWebUIListener(
          'cookieSettingDescriptionChanged',
          description => this.cookieSettingDescription_ = description);
    },

    /**
     * @return {Element}
     * @private
     */
    getControlForSiteSettingsSubpage_() {
      return this.$$(
          this.privacySettingsRedesignEnabled_ ?
              '#permissionsLinkRow' :
              '#site-settings-subpage-trigger');
    },

    /**
     * @return {Element}
     * @private
     */
    getControlForCertificatesSubpage_() {
      return this.$$(
          this.privacySettingsRedesignEnabled_ ? '#securityLinkRow' :
                                                 '#manageCertificates');
    },

    /**
     * @return {Element}
     * @private
     */
    getControlForSecurityKeysSubpage_() {
      return this.$$(
          this.privacySettingsRedesignEnabled_ ?
              '#securityLinkRow' :
              '#security-keys-subpage-trigger');
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
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          settings.PrivacyElementInteractions.IMPROVE_SECURITY);
      this.setPrefValue(
          'safebrowsing.scout_reporting_enabled',
          this.$$('#safeBrowsingReportingToggle').checked);
    },

    /** @private */
    onSafeBrowsingReportingPrefChange_() {
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

    /**
     * Handler for when the sync state is pushed from the browser.
     * @param {?settings.SyncStatus} syncStatus
     * @private
     */
    handleSyncStatus_(syncStatus) {
      this.syncStatus = syncStatus;
    },

    /** @protected */
    currentRouteChanged() {
      this.showClearBrowsingDataDialog_ =
          settings.Router.getInstance().getCurrentRoute() ==
          settings.routes.CLEAR_BROWSER_DATA;
    },


    /**
     * Called when the block autoplay status changes.
     * @param {settings.BlockAutoplayStatus} autoplayStatus
     * @private
     */
    onBlockAutoplayStatusChanged_(autoplayStatus) {
      this.blockAutoplayStatus_ = autoplayStatus;
    },

    /**
     * Updates the block autoplay pref when the toggle is changed.
     * @param {!Event} event
     * @private
     */
    onBlockAutoplayToggleChange_(event) {
      const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
      this.browserProxy_.setBlockAutoplayEnabled(target.checked);
    },

    /**
     * Updates both required block third party cookie preferences.
     * @param {!Event} event
     * @private
     */
    onBlockThirdPartyCookiesToggleChange_(event) {
      const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
      this.setPrefValue('profile.block_third_party_cookies', target.checked);
      this.setPrefValue(
          'profile.cookie_controls_mode',
          target.checked ? settings.CookieControlsMode.ENABLED :
                           settings.CookieControlsMode.DISABLED);
    },

    /**
     * Records changes made to the "can a website check if you have saved
     * payment methods" setting for logging, the logic of actually changing the
     * setting is taken care of by the webUI pref.
     * @private
     */
    onCanMakePaymentChange_() {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          settings.PrivacyElementInteractions.PAYMENT_METHOD);
    },

    /** @private */
    onManageCertificatesTap_() {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          settings.PrivacyElementInteractions.MANAGE_CERTIFICATES);
      // <if expr="use_nss_certs">
      settings.Router.getInstance().navigateTo(settings.routes.CERTIFICATES);
      // </if>
      // <if expr="is_win or is_macosx">
      this.browserProxy_.showManageSSLCertificates();
      // </if>
    },

    /**
     * Records changes made to the network prediction setting for logging, the
     * logic of actually changing the setting is taken care of by the webUI
     * pref.
     * @private
     */
    onNetworkPredictionChange_() {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          settings.PrivacyElementInteractions.NETWORK_PREDICTION);
    },

    /**
     * This is a workaround to connect the remove all button to the subpage.
     * @private
     */
    onRemoveAllCookiesFromSite_() {
      const node = /** @type {?SiteDataDetailsSubpageElement} */ (
          this.$$('site-data-details-subpage'));
      if (node) {
        node.removeAll();
      }
    },

    /** @private */
    onSiteDataTap_() {
      settings.Router.getInstance().navigateTo(
          settings.routes.SITE_SETTINGS_SITE_DATA);
    },

    /** @private */
    onSiteSettingsTap_() {
      settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
    },

    /** @private */
    onSafeBrowsingToggleChange_: function() {
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          settings.PrivacyElementInteractions.SAFE_BROWSING);
    },

    /** @private */
    onClearBrowsingDataTap_() {
      this.tryShowHatsSurvey_();

      settings.Router.getInstance().navigateTo(
          settings.routes.CLEAR_BROWSER_DATA);
    },

    /** @private */
    onCookiesClick_() {
      this.tryShowHatsSurvey_();

      settings.Router.getInstance().navigateTo(settings.routes.COOKIES);
    },

    /** @private */
    onDialogClosed_() {
      settings.Router.getInstance().navigateTo(
          assert(settings.routes.CLEAR_BROWSER_DATA.parent));
      cr.ui.focusWithoutInk(assert(this.$$('#clearBrowsingData')));
    },

    /** @private */
    onPermissionsPageClick_() {
      this.tryShowHatsSurvey_();

      settings.Router.getInstance().navigateTo(settings.routes.SITE_SETTINGS);
    },

    /** @private */
    onSecurityKeysTap_() {
      settings.Router.getInstance().navigateTo(settings.routes.SECURITY_KEYS);
    },

    /** @private */
    onSecurityPageClick_() {
      this.tryShowHatsSurvey_();

      settings.Router.getInstance().navigateTo(settings.routes.SECURITY);
    },

    /** @private */
    getProtectedContentLabel_(value) {
      return value ? this.i18n('siteSettingsProtectedContentEnable') :
                     this.i18n('siteSettingsBlocked');
    },

    /** @private */
    getProtectedContentIdentifiersLabel_(value) {
      return value ?
          this.i18n('siteSettingsProtectedContentEnableIdentifiers') :
          this.i18n('siteSettingsBlocked');
    },

    /** @private */
    tryShowHatsSurvey_() {
      settings.HatsBrowserProxyImpl.getInstance().tryShowSurvey();
    },
  });

  // #cr_define_end
  return {BlockAutoplayStatus};
});
