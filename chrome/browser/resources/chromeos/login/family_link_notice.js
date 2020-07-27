// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Family Link Notice screen.
 */

Polymer({
  is: 'family-link-notice',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {},

  ready() {
    this.initializeLoginScreen('FamilyLinkNoticeScreen', {
      resetAllowed: true,
    });
  },

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  /**
   * On-tap event handler for Continue button.
   *
   * @private
   */
  onContinueButtonPressed_() {
    this.userActed('continue');
  },

});
