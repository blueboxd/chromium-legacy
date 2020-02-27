// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-secure-dns' is a setting that allows the secure DNS
 * mode and secure DNS resolvers to be configured.
 *
 * The underlying secure DNS prefs are not read directly since the setting is
 * meant to represent the current state of the host resolver, which depends not
 * only on the prefs but also a few other factors (e.g. whether we've detected a
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
     * Mirroring the secure DNS mode enum so that it can be used from HTML
     * bindings.
     * @private {!settings.SecureDnsMode}
     */
    secureDnsModeEnum_: {
      type: Object,
      value: settings.SecureDnsMode,
    },

    /**
     * The setting sublabel.
     * @private
     */
    secureDnsDescription_: String,

    /**
     * Represents whether the main toggle for the secure DNS setting is switched
     * on or off.
     * @private {!chrome.settingsPrivate.PrefObject}
     */
    secureDnsToggle_: {
      type: Object,
      value() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        });
      },
    },

    /**
     * Whether the radio buttons should be shown.
     * @private
     */
    showRadioGroup_: Boolean,

    /**
     * Represents the selected radio button. Should always have a value of
     * 'automatic' or 'secure'.
     * @private {!settings.SecureDnsMode}
     */
    secureDnsRadio_: {
      type: String,
      value: settings.SecureDnsMode.AUTOMATIC,
    },

    /**
     * List of secure DNS resolvers to display in dropdown menu.
     * @private {!Array<!settings.ResolverOption>}
     */
    resolverOptions_: Array,

    /**
     * Whether the privacy policy line should be displayed.
     * @private
     */
    showPrivacyPolicyLine_: Boolean,

    /**
     * String displaying the privacy policy of the resolver selected in the
     * dropdown menu.
     * @private
     */
    privacyPolicyString_: String,
  },

  /** @private {?settings.PrivacyPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.PrivacyPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    // Fetch the options for the dropdown menu before configuring the setting
    // to match the underlying host resolver configuration.
    this.browserProxy_.getSecureDnsResolverList().then(resolvers => {
      this.resolverOptions_ = resolvers;
      this.browserProxy_.getSecureDnsSetting().then(
          this.onSecureDnsPrefsChanged_.bind(this));

      // Listen to changes in the host resolver configuration and update the
      // UI representation to match. (Changes to the host resolver configuration
      // may be generated in ways other than direct UI manipulation).
      this.addWebUIListener(
          'secure-dns-setting-changed',
          this.onSecureDnsPrefsChanged_.bind(this));
    });
  },

  /**
   * Update the UI representation to match the underlying host resolver
   * configuration.
   * @param {!settings.SecureDnsSetting} setting
   * @private
   */
  onSecureDnsPrefsChanged_: function(setting) {
    switch (setting.mode) {
      case settings.SecureDnsMode.SECURE:
        this.set('secureDnsToggle_.value', true);
        this.secureDnsRadio_ = settings.SecureDnsMode.SECURE;
        // Only update the selected dropdown item if the user is in secure
        // mode. Otherwise, we may be losing a selection that hasn't been
        // pushed yet to prefs.
        this.updateTemplatesRepresentation_(setting.templates);
        this.updatePrivacyPolicyLine_();
        break;
      case settings.SecureDnsMode.AUTOMATIC:
        this.set('secureDnsToggle_.value', true);
        this.secureDnsRadio_ = settings.SecureDnsMode.AUTOMATIC;
        break;
      case settings.SecureDnsMode.OFF:
        this.set('secureDnsToggle_.value', false);
        break;
      default:
        assertNotReached('Received unknown secure DNS mode');
    }

    this.updateManagementView_(setting.managementMode);
  },

  /**
   * Updates the underlying secure DNS mode pref based on the new toggle
   * selection (and the underlying radio button if the toggle has just been
   * enabled).
   * @private
   */
  onToggleChanged_: function() {
    this.showRadioGroup_ =
        /** @type {boolean} */ (this.secureDnsToggle_.value);
    this.updateDnsPrefs_(
        this.secureDnsToggle_.value ? this.secureDnsRadio_ :
                                      settings.SecureDnsMode.OFF);
  },

  /**
   * Updates the underlying secure DNS prefs based on the newly selected radio
   * button. This should only be called from the HTML.
   * @param {!CustomEvent<{value: !settings.SecureDnsMode}>} event
   * @private
   */
  onRadioSelectionChanged_: function(event) {
    this.updateDnsPrefs_(event.detail.value);
  },

  /**
   * Helper method for updating the underlying secure DNS prefs based on the
   * provided mode.
   * @param {!settings.SecureDnsMode} mode
   * @private
   */
  updateDnsPrefs_: function(mode) {
    switch (mode) {
      case settings.SecureDnsMode.SECURE:
        // If going to secure mode, set the templates pref first to prevent the
        // stub resolver config from being momentarily invalid.
        this.setPrefValue(
            'dns_over_https.templates', this.$.secureResolverSelect.value);
        this.setPrefValue('dns_over_https.mode', mode);
        break;
      case settings.SecureDnsMode.AUTOMATIC:
      case settings.SecureDnsMode.OFF:
        // If going to automatic or off mode, set the mode pref first to avoid
        // clearing the dropdown selection when the templates pref is cleared.
        this.setPrefValue('dns_over_https.mode', mode);
        this.setPrefValue('dns_over_https.templates', '');
        break;
      default:
        assertNotReached('Received unknown secure DNS mode');
    }
  },

  /**
   * Prevent interactions with the dropdown menu from causing the corresponding
   * radio button to be selected.
   * @param {!Event} event
   * @private
   */
  stopDropdownEventPropagation_: function(event) {
    event.stopPropagation();
  },

  /**
   * Updates the underlying secure DNS templates pref based on the selected
   * resolver and displays the corresponding privacy policy.
   * @private
   */
  onDropdownSelectionChanged_: function() {
    // If we're already in secure mode, update the templates pref.
    if (this.secureDnsRadio_ === settings.SecureDnsMode.SECURE) {
      this.updateDnsPrefs_(settings.SecureDnsMode.SECURE);
    }
    this.updatePrivacyPolicyLine_();
  },

  /**
   * Updates the setting to communicate the type of management, if any. The
   * setting is always collapsed if there is any management.
   * @param {!settings.SecureDnsUiManagementMode} managementMode
   * @private
   */
  updateManagementView_: function(managementMode) {
    // If the underlying secure DNS mode pref has an enforced value, communicate
    // that via the toggle pref.
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: this.secureDnsToggle_.value,
    };
    if (this.getPref('dns_over_https.mode').enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = this.getPref('dns_over_https.mode').controlledBy;
      this.secureDnsDescription_ =
          loadTimeData.getString('secureDnsDescription');
    } else {
      // If the secure DNS mode was forcefully overridden by Chrome, provide an
      // explanation in the setting subtitle.
      switch (managementMode) {
        case settings.SecureDnsUiManagementMode.NO_OVERRIDE:
          this.secureDnsDescription_ =
              loadTimeData.getString('secureDnsDescription');
          break;
        case settings.SecureDnsUiManagementMode.DISABLED_MANAGED:
          pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
          this.secureDnsDescription_ =
              loadTimeData.getString('secureDnsDisabledForManagedEnvironment');
          break;
        case settings.SecureDnsUiManagementMode.DISABLED_PARENTAL_CONTROLS:
          pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
          this.secureDnsDescription_ =
              loadTimeData.getString('secureDnsDisabledForParentalControl');
          break;
        default:
          assertNotReached(
              'Received unknown secure DNS management mode ' + managementMode);
      }
    }
    this.secureDnsToggle_ = pref;

    if (this.secureDnsToggle_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      this.showRadioGroup_ = false;
    } else {
      this.showRadioGroup_ =
          /** @type {boolean} */ (this.secureDnsToggle_.value);
    }
  },

  /**
   * Updates the UI to represent the given secure DNS templates.
   * @param {Array<string>} secureDnsTemplates List of secure DNS templates in
   *     the current host resolver configuration.
   * @private
   */
  updateTemplatesRepresentation_: function(secureDnsTemplates) {
    // If there is exactly one template and it is one of the dropdown options,
    // select that option.
    if (secureDnsTemplates.length === 1) {
      const resolver =
          this.resolverOptions_.find(r => r.value === secureDnsTemplates[0]);
      if (resolver) {
        this.$.secureResolverSelect.value = resolver.value;
        return;
      }
    }

    // Otherwise, select the custom option.
    this.$.secureResolverSelect.value = 'custom';
  },

  /**
   * Displays the privacy policy corresponding to the selected dropdown resolver
   * or hides the privacy policy line if a custom resolver is selected.
   * @private
   */
  updatePrivacyPolicyLine_: function() {
    // If the selected item is the custom provider option, hide the privacy
    // policy line.
    if (this.$.secureResolverSelect.value === 'custom') {
      this.showPrivacyPolicyLine_ = false;
      return;
    }

    // Otherwise, display the corresponding privacy policy.
    this.showPrivacyPolicyLine_ = true;
    const resolver = this.resolverOptions_.find(
        r => r.value === this.$.secureResolverSelect.value);
    if (!resolver) {
      return;
    }

    this.privacyPolicyString_ = loadTimeData.substituteString(
        loadTimeData.getString('secureDnsSecureDropdownModePrivacyPolicy'),
        resolver.policy);
  },
});
