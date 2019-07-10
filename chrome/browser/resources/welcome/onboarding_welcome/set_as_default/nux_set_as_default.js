// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'nux-set-as-default',

  behaviors: [
    WebUIListenerBehavior,
    welcome.NavigationBehavior,
  ],

  properties: {
    /** @type {welcome.stepIndicatorModel} */
    indicatorModel: Object,

    // <if expr="is_win">
    isWin10: {
      type: Boolean,
      value: loadTimeData.getBoolean('is_win10'),
    },
    // </if>
  },

  /** @private {welcome.NuxSetAsDefaultProxy} */
  browserProxy_: null,

  /** @private {boolean} */
  finalized_: false,

  /** @override */
  ready: function() {
    this.browserProxy_ = welcome.NuxSetAsDefaultProxyImpl.getInstance();

    this.addWebUIListener(
        'browser-default-state-changed',
        this.onDefaultBrowserChange_.bind(this));
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.browserProxy_.recordPageShown();
  },

  onRouteExit: function() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.browserProxy_.recordNavigatedAwayThroughBrowserHistory();
  },

  onRouteUnload: function() {
    if (this.finalized_) {
      return;
    }
    this.finalized_ = true;
    this.browserProxy_.recordNavigatedAway();
  },

  /** @private */
  onDeclineClick_: function() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordSkip();
    this.finished_();
  },

  /** @private */
  onSetDefaultClick_: function() {
    if (this.finalized_) {
      return;
    }

    this.browserProxy_.recordBeginSetDefault();
    this.browserProxy_.setAsDefault();
  },

  /**
   * Automatically navigate to the next onboarding step once default changed.
   * @param {!welcome.DefaultBrowserInfo} status
   * @private
   */
  onDefaultBrowserChange_: function(status) {
    if (status.isDefault) {
      this.browserProxy_.recordSuccessfullySetDefault();
      // Triggers toast in the containing welcome-app.
      this.fire('default-browser-change');
      this.finished_();
      return;
    }

    // <if expr="is_macosx">
    // On Mac OS, we do not get a notification when the default browser changes.
    // This will fake the notification.
    window.setTimeout(() => {
      this.browserProxy_.requestDefaultBrowserState().then(
          this.onDefaultBrowserChange_.bind(this));
    }, 100);
    // </if>
  },

  /** @private */
  finished_: function() {
    this.finalized_ = true;

    welcome.navigateToNextStep();
  },
});
