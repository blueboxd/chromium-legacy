// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'signin-view',

  behaviors: [welcome.NavigationBehavior],

  /** @private {boolean} */
  finalized_: false,

  /** @private {?welcome.WelcomeBrowserProxy} */
  welcomeBrowserProxy_: null,

  /** @private {?welcome.SigninViewProxy} */
  signinViewProxy_: null,

  /** @override */
  ready: function() {
    this.welcomeBrowserProxy_ = welcome.WelcomeBrowserProxyImpl.getInstance();
    this.signinViewProxy_ = welcome.SigninViewProxyImpl.getInstance();
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.signinViewProxy_.recordPageShown();
  },

  onRouteExit: function() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAwayThroughBrowserHistory();
  },

  onRouteUnload: function() {
    // URL is expected to change when signing in or skipping.
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.signinViewProxy_.recordNavigatedAway();
  },

  /** private */
  onSignInClick_: function() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSignIn();
    this.welcomeBrowserProxy_.handleActivateSignIn(null);
  },

  /** @private */
  onNoThanksClick_: function() {
    this.finalized_ = true;
    this.signinViewProxy_.recordSkip();
    this.welcomeBrowserProxy_.handleUserDecline();
  }
});
