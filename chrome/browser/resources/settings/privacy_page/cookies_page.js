// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-cookies-page' is the settings page containing cookies
 * settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '../controls/settings_toggle_button.m.js';
import '../icons.m.js';
import '../prefs/prefs.m.js';
import '../settings_shared_css.m.js';
import '../site_settings/site_list.js';
import './collapse_radio_button.js';
import './do_not_track_toggle.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';
import {ContentSetting, ContentSettingsTypes, CookieControlsMode, SiteSettingSource} from '../site_settings/constants.js';
import {ContentSettingProvider, CookieControlsManagedState, DefaultContentSetting, SiteSettingsPrefsBrowserProxy, SiteSettingsPrefsBrowserProxyImpl} from '../site_settings/site_settings_prefs_browser_proxy.js';

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

  _template: html`{__html_template__}`,

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
      value: ContentSetting,
    },

    /**
     * @private {!ContentSettingsTypes}
     */
    cookiesContentSettingType_: {
      type: String,
      value: ContentSettingsTypes.COOKIES,
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

  /** @type {?SiteSettingsPrefsBrowserProxy} */
  browserProxy_: null,

  /** @type {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  created() {
    // Used during property initialisation so must be set in created.
    this.browserProxy_ = SiteSettingsPrefsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();

    this.addWebUIListener('contentSettingCategoryChanged', category => {
      if (category === ContentSettingsTypes.COOKIES) {
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
    assert(routes.SITE_SETTINGS_SITE_DATA);
    this.focusConfig.set(routes.SITE_SETTINGS_SITE_DATA.path, () => {
      focusWithoutInk(assert(this.$$('#site-data-trigger')));
    });
  },

  /** @private */
  onSiteDataClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DATA);
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
            ContentSettingsTypes.COOKIES);

    // Set control state.
    if (contentSetting.setting === ContentSetting.BLOCK) {
      this.cookiesControlRadioSelected_ = CookiesControl.BLOCK_ALL;
    } else if (blockThirdParty.value) {
      this.cookiesControlRadioSelected_ =
          this.cookiesControlEnum_.BLOCK_THIRD_PARTY;
    } else if (
        this.improvedCookieControlsEnabled_ &&
        controlMode.value === CookieControlsMode.INCOGNITO_ONLY) {
      this.cookiesControlRadioSelected_ =
          this.cookiesControlEnum_.BLOCK_THIRD_PARTY_INCOGNITO;
    } else {
      this.cookiesControlRadioSelected_ = CookiesControl.ALLOW_ALL;
    }
    this.clearOnExitDisabled_ = contentSetting.setting === ContentSetting.BLOCK;

    // Update virtual preference for the clear on exit toggle.
    // TODO(crbug.com/1063265): Create toggle that can directly accept state.
    this.clearOnExitPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: contentSetting.setting === ContentSetting.BLOCK ?
          false :
          contentSetting.setting === ContentSetting.SESSION_ONLY,
      controlledBy: this.getControlledBy_(contentSetting),
      enforcement: this.getEnforced_(contentSetting),
    };

    // Set the exception lists to read only if the content setting is managed.
    // Display of managed state for individual entries will be handled by each
    // site-list-entry.
    this.exceptionListsReadOnly_ =
        contentSetting.source === SiteSettingSource.POLICY;

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
   * @return {!ContentSetting}
   * @private
   */
  computeClearOnExitSetting_() {
    return this.$.clearOnExit.checked ? ContentSetting.SESSION_ONLY :
                                        ContentSetting.ALLOW;
  },

  /**
   * Set required preferences base on control mode and content setting.
   * @param {!CookieControlsMode} controlsMode
   * @param {!ContentSetting} contentSetting
   * @private
   */
  setAllPrefs_(controlsMode, contentSetting) {
    this.setPrefValue('profile.cookie_controls_mode', controlsMode);
    this.setPrefValue(
        'profile.block_third_party_cookies',
        controlsMode == CookieControlsMode.ENABLED);
    this.browserProxy_.setDefaultValueForContentType(
        ContentSettingsTypes.COOKIES, contentSetting);
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
          CookieControlsMode.DISABLED, this.computeClearOnExitSetting_());
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_ALL);
    } else if (
        event.detail.value === CookiesControl.BLOCK_THIRD_PARTY_INCOGNITO) {
      this.setAllPrefs_(
          CookieControlsMode.INCOGNITO_ONLY, this.computeClearOnExitSetting_());
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_INCOGNITO);
    } else if (event.detail.value === CookiesControl.BLOCK_THIRD_PARTY) {
      this.setAllPrefs_(
          CookieControlsMode.ENABLED, this.computeClearOnExitSetting_());
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_THIRD);
    } else {  // CookiesControl.BLOCK_ALL
      this.setAllPrefs_(CookieControlsMode.ENABLED, ContentSetting.BLOCK);
      this.metricsBrowserProxy_.recordSettingsPageHistogram(
          PrivacyElementInteractions.COOKIES_BLOCK);
    }
  },

  /** @private */
  onClearOnExitChange_() {
    this.browserProxy_.setDefaultValueForContentType(
        ContentSettingsTypes.COOKIES, this.computeClearOnExitSetting_());
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.COOKIES_SESSION);
  },

  /**
   * Records changes made to the network prediction setting for logging, the
   * logic of actually changing the setting is taken care of by the
   * net.network_prediction_options pref.
   * @private
   */
  onNetworkPredictionChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.NETWORK_PREDICTION);
  },
});
