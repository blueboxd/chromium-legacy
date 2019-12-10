// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

Polymer({
  is: 'marketing-opt-in',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  /**
   * This is 'on-tap' event handler for 'AcceptAndContinue/Next' buttons.
   * @private
   */
  onAllSet_: function() {
    chrome.send('login.MarketingOptInScreen.allSet', [
      this.$.playUpdatesOption.checked, this.$.chromebookUpdatesOption.checked
    ]);
  },
});
