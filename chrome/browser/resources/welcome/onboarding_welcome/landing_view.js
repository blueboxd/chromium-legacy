// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'landing-view',

  behaviors: [welcome.NavigationBehavior],

  properties: {
    /** @private */
    signinAllowed_: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('signinAllowed'),
    }
  },

  /** @private {?welcome.LandingViewProxy} */
  landingViewProxy_: null,

  /** @private {boolean} */
  finalized_: false,

  /** @override */
  ready() {
    this.landingViewProxy_ = welcome.LandingViewProxyImpl.getInstance();
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.landingViewProxy_.recordPageShown();
  },

  onRouteUnload: function() {
    // Clicking on 'Returning user' will change the URL.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.landingViewProxy_.recordNavigatedAway();
  },

  /** @private */
  onExistingUserClick_: function() {
    this.finalized_ = true;
    this.landingViewProxy_.recordExistingUser();
    if (this.signinAllowed_) {
      welcome.WelcomeBrowserProxyImpl.getInstance().handleActivateSignIn(
        'chrome://welcome/returning-user');
    } else {
      welcome.navigateTo(welcome.Routes.RETURNING_USER, 1);
    }
  },

  /** @private */
  onNewUserClick_: function() {
    this.finalized_ = true;
    this.landingViewProxy_.recordNewUser();
    welcome.navigateTo(welcome.Routes.NEW_USER, 1);
  }
});
