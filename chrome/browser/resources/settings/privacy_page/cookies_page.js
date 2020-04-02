// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cookies-page' is the settings page containing cookies
 * settings.
 */

(function() {
/**
 * Enumeration of all cookies radio controls.
 * @enum {string}
 */
const CookiesControl = {
  ALLOW_ALL: 'allow-all',
  BLOCK_THIRD_PARTY_INCOGNITO: 'block-third-party-incognito',
  BLOCK_THIRD_PARTY: 'block-third-party',
  BLOCK_ALL: 'block-all',
};

/**
 * Must be kept in sync with the C++ enum of the same name (see
 * chrome/browser/net/prediction_options.h).
 * @enum {number}
 */
const NetworkPredictionOptions = {
  ALWAYS: 0,
  WIFI_ONLY: 1,
  NEVER: 2,
  DEFAULT: 1,
};

Polymer({
  is: 'settings-cookies-page',

  behaviors: [PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Current search term.
     */
    searchTerm: {
      type: String,
      notify: true,
      value: '',
    },

    /**
     * Valid cookie states.
     * @private
     */
    cookiesControlEnum_: {
      type: Object,
      value: CookiesControl,
    },

    /** @private */
    cookiesControlRadioSelected_: String,

    /**
     * Used for HTML bindings. This is defined as a property rather than
     * within the ready callback, because the value needs to be available
     * before local DOM initialization - otherwise, the toggle has unexpected
     * behavior.
     * @private {!NetworkPredictionOptions}
     */
    networkPredictionUncheckedValue_: {
      type: Number,
      value: NetworkPredictionOptions.NEVER,
    },

    // A "virtual" preference that is use to control the state of the
    // clear on exit toggle.
    /** @private {chrome.settingsPrivate.PrefObject} */
    clearOnExitPref_: {
      type: Object,
      value() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /** @private */
    clearOnExitDisabled_: {
      type: Boolean,
      notify: true,
    },

    /** @private */
    contentSetting_: {
      type: Object,
      value: settings.ContentSetting,
    },

    /**
     * @private {!settings.ContentSettingsTypes}
     */
    cookiesContentSettingType_: {
      type: String,
      value: settings.ContentSettingsTypes.COOKIES,
    },

    /**
     * Whether the cookie content setting is managed.
     * @private
     */
    cookiesContentSettingManaged_: {
      type: Boolean,
      notify: false,
    },

    /** @private {!CookieControlsManagedState} */
    cookieControlsManagedState_: {
      type: Object,
      notify: true,
    },

    /** @private */
    improvedCookieControlsEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('improvedCookieControlsEnabled');
      }
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  observers: [
    'updateCookiesControls_(prefs.profile.cookie_controls_mode,' +
        'prefs.profile.block_third_party_cookies)',
  ],

  /** @type {?settings.SiteSettingsPrefsBrowserProxy} */
  browserProxy_: null,

  /** @type {?settings.MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  created() {
    // Used during property initialisation so must be set in created.
    this.browserProxy_ =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.metricsBrowserProxy_ = settings.MetricsBrowserProxyImpl.getInstance();

    this.addWebUIListener('contentSettingCategoryChanged', category => {
      if (category === settings.ContentSettingsTypes.COOKIES) {
        this.updateCookiesControls_();
      }
    });
  },

  /*
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    assert(!oldConfig);
    assert(settings.routes.SITE_SETTINGS_SITE_DATA);
    this.focusConfig.set(settings.routes.SITE_SETTINGS_SITE_DATA.path, () => {
      cr.ui.focusWithoutInk(assert(this.$$('#site-data-trigger')));
    });
  },

  /** @private */
  onSiteDataClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DATA);
  },

  /**
   * Updates the cookies control radio toggle to represent the current cookie
   * state.
   * @private
   */
  async updateCookiesControls_() {
    const controlMode = this.getPref('profile.cookie_controls_mode');
    const blockThirdParty = this.getPref('profile.block_third_party_cookies');
    const contentSetting =
        await this.browserProxy_.getDefaultValueForContentType(
            settings.ContentSettingsTypes.COOKIES);

    // Set control state.
    if (contentSetting.setting === settings.ContentSetting.BLOCK) {
      this.cookiesControlRadioSelected_ = CookiesControl.BLOCK_ALL;
    } else if (blockThirdParty.value) {
      this.cookiesControlRadioSelected_ =
          this.cookiesControlEnum_.BLOCK_THIRD_PARTY;
    } else if (this.improvedCookieControlsEnabled_ &&
        controlMode.value === settings.CookieControlsMode.INCOGNITO_ONLY) {
      this.cookiesControlRadioSelected_ =
          this.cookiesControlEnum_.BLOCK_THIRD_PARTY_INCOGNITO;
    } else {
      this.cookiesControlRadioSelected_ = CookiesControl.ALLOW_ALL;
    }
    this.clearOnExitDisabled_ =
        contentSetting.setting === settings.ContentSetting.BLOCK;

    // Update virtual preference for the clear on exit toggle.
    // TODO(crbug.com/1063265): Create toggle that can directly accept state.
    this.clearOnExitPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: contentSetting.setting === settings.ContentSetting.BLOCK ?
          false :
          contentSetting.setting === settings.ContentSetting.SESSION_ONLY,
      controlledBy: this.getControlledBy_(contentSetting),
      enforcement: this.getEnforced_(contentSetting),
    };

    // Set the exception lists to read only if the content setting is managed.
    // Display of managed state for individual entries will be handled by each
    // site-list-entry.
    this.exceptionListsReadOnly_ =
        contentSetting.source === settings.SiteSettingSource.POLICY;

    // Retrieve and update remaining controls managed state.
    this.cookieControlsManagedState_ =
        await this.browserProxy_.getCookieControlsManagedState();
  },

  /**
   * Get an appropriate pref source for a DefaultContentSetting.
   * @param {!DefaultContentSetting} provider
   * @return {!chrome.settingsPrivate.ControlledBy}
   * @private
   */
  getControlledBy_({source}) {
    switch (source) {
      case ContentSettingProvider.POLICY:
        return chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
      case ContentSettingProvider.SUPERVISED_USER:
        return chrome.settingsPrivate.ControlledBy.PARENT;
      case ContentSettingProvider.EXTENSION:
        return chrome.settingsPrivate.ControlledBy.EXTENSION;
      default:
        return chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }
  },

  /**
   * Get an appropriate pref enforcement setting for a DefaultContentSetting.
   * @param {!DefaultContentSetting} provider
   * @return {!chrome.settingsPrivate.Enforcement|undefined}
   * @private
   */
  getEnforced_({source}) {
    switch (source) {
      case ContentSettingProvider.POLICY:
        return chrome.settingsPrivate.Enforcement.ENFORCED;
      case ContentSettingProvider.SUPERVISED_USER:
        return chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED;
      default:
        return undefined;
    }
  },

  /**
   * @return {!settings.ContentSetting}
   * @private
   */
  computeClearOnExitSetting_() {
    return this.$.clearOnExit.checked ? settings.ContentSetting.SESSION_ONLY :
                                        settings.ContentSetting.ALLOW;
  },

  /**
   * Set required preferences base on control mode and content setting.
   * @param {!settings.CookieControlsMode} controlsMode
   * @param {!settings.ContentSetting} contentSetting
   * @private
   */
  setAllPrefs_(controlsMode, contentSetting) {
    this.setPrefValue('profile.cookie_controls_mode', controlsMode);
    this.setPrefValue(
        'profile.block_third_party_cookies',
        controlsMode == settings.CookieControlsMode.ENABLED);
    this.browserProxy_.setDefaultValueForContentType(
        settings.ContentSettingsTypes.COOKIES, contentSetting);
  },

  /**
   * Updates the various underlying cookie prefs and content settings
   * based on the newly selected radio button.
   * @param {!CustomEvent<{value:!CookiesControl}>} event
   * @private
   */
  onCookiesControlRadioChange_(event) {
    if (event.detail.value === CookiesControl.ALLOW_ALL) {
      this.setAllPrefs_(
          settings.CookieControlsMode.DISABLED,
          this.computeClearOnExitSetting_());
    } else if (
        event.detail.value === CookiesControl.BLOCK_THIRD_PARTY_INCOGNITO) {
      this.setAllPrefs_(
          settings.CookieControlsMode.INCOGNITO_ONLY,
          this.computeClearOnExitSetting_());
    } else if (event.detail.value === CookiesControl.BLOCK_THIRD_PARTY) {
      this.setAllPrefs_(
          settings.CookieControlsMode.ENABLED,
          this.computeClearOnExitSetting_());
    } else {  // CookiesControl.BLOCK_ALL
      this.setAllPrefs_(
          settings.CookieControlsMode.ENABLED, settings.ContentSetting.BLOCK);
    }
  },

  /** @private */
  onClearOnExitChange_() {
    this.browserProxy_.setDefaultValueForContentType(
        settings.ContentSettingsTypes.COOKIES,
        this.computeClearOnExitSetting_());
  },

  /**
   * Records changes made to the network prediction setting for logging, the
   * logic of actually changing the setting is taken care of by the
   * net.network_prediction_options pref.
   * @private
   */
  onNetworkPredictionChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        settings.PrivacyElementInteractions.NETWORK_PREDICTION);
  },
});
})();
