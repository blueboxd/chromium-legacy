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

      /** @private */
      showDoNotTrackDialog_: {
        type: Boolean,
        value: false,
      },

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
       * Whether the more settings list is opened.
       * @private
       */
      moreOpened_: {
        type: Boolean,
        value: false,
      },

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
      enableWebXrContentSetting_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableWebXrContentSetting'),
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

    /** @override */
    ready() {
      this.ContentSettingsTypes = settings.ContentSettingsTypes;
      this.ChooserType = settings.ChooserType;

      this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();

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
     * @param {!Event} event
     * @private
     */
    onDoNotTrackDomChange_(event) {
      if (this.showDoNotTrackDialog_) {
        this.maybeShowDoNotTrackDialog_();
      }
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
     * Records changes made to the "can a website check if you have saved
     * payment methods" setting for logging, the logic of actually changing the
     * setting is taken care of by the webUI pref.
     * @private
     */
    onCanMakePaymentChange_() {
      this.browserProxy_.recordSettingsPageHistogram(
          settings.SettingsPageInteractions.PRIVACY_PAYMENT_METHOD);
    },

    /**
     * Handles the change event for the do-not-track toggle. Shows a
     * confirmation dialog when enabling the setting.
     * @param {!Event} event
     * @private
     */
    onDoNotTrackChange_(event) {
      this.browserProxy_.recordSettingsPageHistogram(
          settings.SettingsPageInteractions.PRIVACY_DO_NOT_TRACK);
      const target = /** @type {!SettingsToggleButtonElement} */ (event.target);
      if (!target.checked) {
        // Always allow disabling the pref.
        target.sendPrefChange();
        return;
      }
      this.showDoNotTrackDialog_ = true;
      // If the dialog has already been stamped, show it. Otherwise it will be
      // shown in onDomChange_.
      this.maybeShowDoNotTrackDialog_();
    },

    /** @private */
    maybeShowDoNotTrackDialog_() {
      const dialog = this.$$('#confirmDoNotTrackDialog');
      if (dialog && !dialog.open) {
        dialog.showModal();
      }
    },

    /** @private */
    closeDoNotTrackDialog_() {
      this.$$('#confirmDoNotTrackDialog').close();
      this.showDoNotTrackDialog_ = false;
    },

    /** @private */
    onDoNotTrackDialogClosed_() {
      cr.ui.focusWithoutInk(this.$.doNotTrack);
    },

    /**
     * Handles the shared proxy confirmation dialog 'Confirm' button.
     * @private
     */
    onDoNotTrackDialogConfirm_() {
      /** @type {!SettingsToggleButtonElement} */ (this.$.doNotTrack)
          .sendPrefChange();
      this.closeDoNotTrackDialog_();
    },

    /**
     * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
     * event.
     * @private
     */
    onDoNotTrackDialogCancel_() {
      /** @type {!SettingsToggleButtonElement} */ (this.$.doNotTrack)
          .resetToPrefValue();
      this.closeDoNotTrackDialog_();
    },

    /** @private */
    onManageCertificatesTap_() {
      // <if expr="use_nss_certs">
      settings.Router.getInstance().navigateTo(settings.routes.CERTIFICATES);
      // </if>
      // <if expr="is_win or is_macosx">
      this.browserProxy_.showManageSSLCertificates();
      // </if>
      this.browserProxy_.recordSettingsPageHistogram(
          settings.SettingsPageInteractions.PRIVACY_MANAGE_CERTIFICATES);
    },

    /**
     * Records changes made to the network prediction setting for logging, the
     * logic of actually changing the setting is taken care of by the webUI
     * pref.
     * @private
     */
    onNetworkPredictionChange_() {
      this.browserProxy_.recordSettingsPageHistogram(
          settings.SettingsPageInteractions.PRIVACY_NETWORK_PREDICTION);
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
      this.browserProxy_.recordSettingsPageHistogram(
          settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS);
    },

    /** @private */
    onClearBrowsingDataTap_() {
      settings.Router.getInstance().navigateTo(
          settings.routes.CLEAR_BROWSER_DATA);
      this.browserProxy_.recordSettingsPageHistogram(
          settings.SettingsPageInteractions.PRIVACY_CLEAR_BROWSING_DATA);
    },

    /** @private */
    onDialogClosed_() {
      settings.Router.getInstance().navigateTo(
          settings.routes.CLEAR_BROWSER_DATA.parent);
      cr.ui.focusWithoutInk(assert(this.$.clearBrowsingData));
    },

    /** @private */
    onSecurityKeysTap_() {
      settings.Router.getInstance().navigateTo(settings.routes.SECURITY_KEYS);
      this.browserProxy_.recordSettingsPageHistogram(
          settings.SettingsPageInteractions.PRIVACY_SECURITY_KEYS);
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
  });

  return {BlockAutoplayStatus};
});
