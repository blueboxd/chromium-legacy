// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Packaged License screen.
 */

Polymer({
  is: 'packaged-license-screen',

  behaviors: [I18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  properties: {

  },

  ready: function() {
    this.initializeLoginScreen('PackagedLicenseScreen', {
      resetAllowed: true,
    });
  },

  /**
   * On-tap event handler for Don't Enroll button.
   *
   * @private
   */
  onDontEnrollButtonPressed_: function() {
    chrome.send('login.PackagedLicenseScreen.userActed', ['dont-enroll']);
  },

  /**
   * On-tap event handler for Enroll button.
   *
   * @private
   */
  onEnrollButtonPressed_: function() {
    chrome.send('login.PackagedLicenseScreen.userActed', ['enroll']);
  },

});
