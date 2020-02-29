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
    allSetButtonVisible_: {
      type: Boolean,
      value: true,
    },
  },

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  /** Overridden from LoginScreenBehavior. */
  EXTERNAL_API: [
    'updateAllSetButtonVisibility',
  ],

  /** @override */
  ready() {
    this.initializeLoginScreen('MarketingOptInScreen', {resetAllowed: true});
  },

  /**
   * This is 'on-tap' event handler for 'AcceptAndContinue/Next' buttons.
   * @private
   */
  onAllSet_() {
    chrome.send('login.MarketingOptInScreen.allSet', [
      this.$.playUpdatesOption.checked, this.$.chromebookUpdatesOption.checked
    ]);
  },

  /**
   * @param {boolean} visible Whether the all set button should be shown.
   */
  updateAllSetButtonVisibility(visible) {
    // TODO(mmourgos): Update |this.allSetButtonVisible_| once the accessibility
    // setting to show shelf buttons is added to screen.
  },
});
