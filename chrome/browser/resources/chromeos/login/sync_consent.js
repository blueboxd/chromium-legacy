// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

Polymer({
  is: 'sync-consent',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Flag that determines whether current account type is supervised or not.
     */
    isChildAccount_: { type: Boolean },
    /** @private */
    splitSettingsSyncEnabled_: {
      type: Boolean,
      value: function () {
        return loadTimeData.getBoolean('splitSettingsSyncEnabled');
      },
      readOnly: true,
    },

  },

  /**
   * Set flag isChildAccount_ value.
   * @param is_child_account Boolean
   */
  setIsChildAccount(is_child_account) {
    this.isChildAccount_ = is_child_account;
  },

  /** @override */
  ready() {
    this.updateLocalizedContent();
  },

  focus() {
    let activeScreen = this.getActiveScreen_();
    if (activeScreen)
      activeScreen.focus();
  },

  /**
   * Hides all screens to help switching from one screen to another.
   * @private
   */
  hideAllScreens_() {
    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (let screen of screens)
      screen.hidden = true;
  },

  /**
   * Returns active screen or null if none.
   * @private
   */
  getActiveScreen_() {
    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (let screen of screens) {
      if (!screen.hidden)
        return screen;
    }
    return null;
  },

  /**
   * Shows given screen.
   * @param id String Screen ID.
   * @private
   */
  showScreen_(id) {
    this.hideAllScreens_();

    var screen = this.$[id];
    assert(screen);
    screen.hidden = false;
    screen.show();
    screen.focus();
  },

  /**
   * Reacts to changes in loadTimeData.
   */
  updateLocalizedContent() {
    if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      // SplitSettingsSync version.
      this.showScreen_('splitSettingsSyncConsentDialog');
    } else {
      // Regular version.
      this.showScreen_('syncConsentOverviewDialog');
    }
    this.i18nUpdateLocale();
  },

  /**
   * Continue button click handler for pre-SplitSettingsSync.
   * @private
   */
  onSettingsSaveAndContinue_(e) {
    assert(e.path);
    assert(!loadTimeData.getBoolean('splitSettingsSyncEnabled'));
    if (this.$.reviewSettingsBox.checked) {
      chrome.send('login.SyncConsentScreen.continueAndReview', [
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path)
      ]);
    } else {
      chrome.send('login.SyncConsentScreen.continueWithDefaults', [
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path)
      ]);
    }
  },

  /**
   * Continue button handler for SplitSettingsSync.
   * @param {!Event} event
   * @private
   */
  onSettingsAcceptAndContinue_(event) {
    assert(loadTimeData.getBoolean('splitSettingsSyncEnabled'));
    assert(event.path);
    const enableOsSync = !!this.$.osSyncToggle.checked;
    const reviewBrowserSync = !!this.$.reviewBrowserSyncOptionsBox.checked;
    chrome.send('login.SyncConsentScreen.acceptAndContinue', [
      this.getConsentDescription_(), this.getConsentConfirmation_(event.path),
      enableOsSync, reviewBrowserSync
    ]);
  },

  /**
   * @param {!Array<!HTMLElement>} path Path of the click event. Must contain
   *     a consent confirmation element.
   * @return {string} The text of the consent confirmation element.
   * @private
   */
  getConsentConfirmation_(path) {
    for (let element of path) {
      if (!element.hasAttribute)
        continue;

      if (element.hasAttribute('consent-confirmation'))
        return element.innerHTML.trim();

      // Search down in case of click on a button with description below.
      let labels = element.querySelectorAll('[consent-confirmation]');
      if (labels && labels.length > 0) {
        assert(labels.length == 1);

        let result = '';
        for (let label of labels) {
          result += label.innerHTML.trim();
        }
        return result;
      }
    }
    assertNotReached('No consent confirmation element found.');
    return '';
  },

  /** @return {!Array<string>} Text of the consent description elements. */
  getConsentDescription_() {
    let consentDescription =
      Array.from(this.shadowRoot.querySelectorAll('[consent-description]'))
        .filter(element => element.clientWidth * element.clientHeight > 0)
        .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  },
});
