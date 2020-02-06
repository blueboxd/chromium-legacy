// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-secure-dns' is a setting that allows the secure DNS
 * mode and secure DNS resolvers to be configured.
 *
 * The underlying secure DNS prefs are not read directly since the setting is
 * meant to represent the current state of the host resolver, which depends not
 * only the prefs but also a few other factors (e.g. whether we've detected a
 * managed environment, whether we've detected parental controls, etc). Instead,
 * the setting listens for secure-dns-setting-changed events, which are sent
 * by PrivacyPageBrowserProxy and describe the new host resolver configuration.
 */
Polymer({
  is: 'settings-secure-dns',

  behaviors: [WebUIListenerBehavior, PrefsBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Valid secure DNS modes and their string representations.
     * @private
     */
    prefModeValues_: {
      readOnly: true,
      type: Object,
      value: {
        OFF: 'off',
        AUTOMATIC: 'automatic',
        SECURE: 'secure',
      },
    },

    /**
     * Represents whether the main toggle for the secure DNS setting is switched
     * on or off.
     * @private
     */
    secureDnsToggle_: {
      type: Object,
      value: {value: false},
    },

    /**
     * Represents the selected radio button. Should always have a value of
     * 'automatic' or 'secure'.
     * @private
     */
    secureDnsRadio_: {
      type: String,
      value: 'automatic',
    },
  },

  /** @private {?settings.PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.browserProxy_.getSecureDnsSetting().then(
        this.onSecureDnsPrefsChanged_.bind(this));

    // Listen to changes in the host resolver configuration and update the
    // UI representation to match. (Changes to the host resolver configuration
    // may be generated in ways other than direct UI manipulation).
    this.addWebUIListener(
        'secure-dns-setting-changed', this.onSecureDnsPrefsChanged_.bind(this));
  },

  /**
   * Update the UI representation to match the underlying host resolver
   * configuration.
   * @param {!SecureDnsSetting} setting
   * @private
   */
  onSecureDnsPrefsChanged_: function(setting) {
    switch (setting.mode) {
      case this.prefModeValues_.SECURE:
        this.set('secureDnsToggle_.value', true);
        this.secureDnsRadio_ = this.prefModeValues_.SECURE;
        break;
      case this.prefModeValues_.AUTOMATIC:
        this.set('secureDnsToggle_.value', true);
        this.secureDnsRadio_ = this.prefModeValues_.AUTOMATIC;
        break;
      case this.prefModeValues_.OFF:
        this.set('secureDnsToggle_.value', false);
        break;
      default:
        assertNotReached('Received unknown secure DNS mode');
    }
  },

  /**
   * Updates the underlying secure DNS mode pref based on the new toggle
   * selection (and the underlying radio button if the toggle has just been
   * enabled).
   * @private
   */
  onToggleChanged_: function() {
    this.setPrefValue(
        'dns_over_https.mode',
        this.secureDnsToggle_.value ? this.secureDnsRadio_ :
                                      this.prefModeValues_.OFF);
  },

  /**
   * Updates the underlying secure DNS mode pref based on the newly selected
   * radio button.
   * @param {!CustomEvent<{value: string}>} event
   * @private
   */
  onRadioSelectionChanged_: function(event) {
    this.setPrefValue('dns_over_https.mode', event.detail.value);
  },
});
