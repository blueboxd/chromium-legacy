// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

Polymer({
  is: 'marketing-opt-in',

  properties: {
    isAccessibilitySettingsShown_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the marketing opt in toggles should be shown, which will be the
     * case only if marketing opt in feature is enabled.
     * When this is false, the screen will only contain UI related to the
     * tablet mode gestural navigation settings.
     */
    marketingOptInEnabled_: {
      type: Boolean,
      readOnly: true,
    },
  },

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  /** Overridden from LoginScreenBehavior. */
  EXTERNAL_API: [
    'updateA11yNavigationButtonToggle',
  ],

  /** @override */
  ready() {
    this.initializeLoginScreen('MarketingOptInScreen', {resetAllowed: true});
    this.$['marketingOptInOverviewDialog']
        .querySelector('.marketing-animation')
        .setPlay(true);
  },

  /** Called when dialog is shown */
  onBeforeShow() {
    this.isAccessibilitySettingsShown_ = false;

    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
  },

  /**
   * This is 'on-tap' event handler for 'AcceptAndContinue/Next' buttons.
   * @private
   */
  onGetStarted_() {
    this.$['marketingOptInOverviewDialog']
        .querySelector('.marketing-animation')
        .setPlay(false);
    chrome.send(
        'login.MarketingOptInScreen.onGetStarted',
        [this.$.chromebookUpdatesOption.checked]);
  },

  /**
   * @param {boolean} enabled Whether the a11y setting for shownig shelf
   * navigation buttons is enabled.
   */
  updateA11yNavigationButtonToggle(enabled) {
    this.$.a11yNavButtonToggle.checked = enabled;
  },

  /**
   * This is the 'on-tap' event handler for the accessibility settings link and
   * for the back button on the accessibility page.
   * @private
   */
  onToggleAccessibilityPage_() {
    this.isAccessibilitySettingsShown_ = !this.isAccessibilitySettingsShown_;
    this.$['marketingOptInOverviewDialog']
        .querySelector('.marketing-animation')
        .setPlay(!this.isAccessibilitySettingsShown_);
  },

  /**
   * The 'on-change' event handler for when the a11y navigation button setting
   * is toggled on or off.
   * @private
   */
  onA11yNavButtonsSettingChanged_() {
    chrome.send('login.MarketingOptInScreen.setA11yNavigationButtonsEnabled', [
      this.$.a11yNavButtonToggle.checked
    ]);
  }
});
