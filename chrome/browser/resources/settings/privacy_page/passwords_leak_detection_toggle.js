// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import '../controls/settings_toggle_button.m.js';
import '../prefs/prefs.m.js';
import '../settings_shared_css.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {StoredAccount, SyncBrowserProxyImpl, SyncStatus} from '../people_page/sync_browser_proxy.m.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';

Polymer({
  is: 'settings-passwords-leak-detection-toggle',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {SyncStatus} */
    syncStatus: Object,

    // <if expr="not chromeos">
    /** @private {Array<!StoredAccount>} */
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

    // A "virtual" preference that is use to control the associated toggle
    // independently of the preference it represents. This is updated with
    // the registered setPasswordsLeakDetectionPref_ observer. Changes to
    // the real preference are handled in onPasswordsLeakDetectionChange_
    /** @private {chrome.settingsPrivate.PrefObject} */
    passwordsLeakDetectionPref_: {
      type: Object,
      value() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },
  },

  observers: [
    'setPasswordsLeakDetectionPref_(userSignedIn_,' +
        'passwordsLeakDetectionAvailable_)',
  ],

  /** @override */
  ready() {
    // <if expr="not chromeos">
    const storedAccountsChanged = storedAccounts => this.storedAccounts_ =
        storedAccounts;
    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeUserSignedIn_() {
    return (!!this.syncStatus && !!this.syncStatus.signedIn) ?
        !this.syncStatus.hasError :
        (!!this.storedAccounts_ && this.storedAccounts_.length > 0);
  },

  /**
   * @return {boolean}
   * @private
   */
  computePasswordsLeakDetectionAvailable_() {
    if (this.prefs === undefined) {
      return false;
    }
    return !!this.getPref('profile.password_manager_leak_detection').value;
  },

  /**
   * @return {string}
   * @private
   */
  getPasswordsLeakDetectionSubLabel_() {
    let subLabel = this.i18n('passwordsLeakDetectionGeneralDescription');
    if (!this.userSignedIn_ && this.passwordsLeakDetectionAvailable_) {
      subLabel +=
          ' ' +  // Whitespace is a valid sentence separator w.r.t. i18n.
          this.i18n('passwordsLeakDetectionSignedOutEnabledDescription') +
          // The string appended on the previous line was added as a standalone
          // sentence that did not end with a period. Since here we're appending
          // it to a two-sentence string, with both of those sentences ending
          // with periods, we must add a period at the end.
          // TODO(crbug.com/1032584): After the privacy settings redesign, this
          // string will never appear standalone. Include the period in it.
          this.i18n('sentenceEnd');
    }
    return subLabel;
  },

  /**
   * @return {boolean}
   * @private
   */
  getDisabledLeakDetection_() {
    if (this.prefs === undefined) {
      return false;
    }
    return !this.userSignedIn_ || !this.getPref('safebrowsing.enabled').value ||
        !!this.getPref('safebrowsing.enhanced').value;
  },

  /** @private */
  onPasswordsLeakDetectionChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.PASSWORD_CHECK);
    this.setPrefValue(
        'profile.password_manager_leak_detection',
        this.$.passwordsLeakDetectionCheckbox.checked);
  },

  /** @private */
  setPasswordsLeakDetectionPref_() {
    if (this.prefs === undefined) {
      return;
    }
    const passwordManagerLeakDetectionPref =
        this.getPref('profile.password_manager_leak_detection');
    const prefValue =
        this.userSignedIn_ && this.passwordsLeakDetectionAvailable_;
    this.passwordsLeakDetectionPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: prefValue,
      enforcement: passwordManagerLeakDetectionPref.enforcement,
      controlledBy: passwordManagerLeakDetectionPref.controlledBy,
    };
  },
});
