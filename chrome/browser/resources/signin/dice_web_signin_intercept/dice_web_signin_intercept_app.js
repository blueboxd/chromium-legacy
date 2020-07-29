// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './signin_icons.js';
import './signin_shared_css.js';
import './signin_vars_css.js';
import './strings.m.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AccountInfo, DiceWebSigninInterceptBrowserProxy, DiceWebSigninInterceptBrowserProxyImpl} from './dice_web_signin_intercept_browser_proxy.js';

Polymer({
  is: 'dice-web-signin-intercept-app',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private {AccountInfo} */
    accountInfo_: Object,
  },

  /** @private {?DiceWebSigninInterceptBrowserProxy} */
  diceWebSigninInterceptBrowserProxy_: null,

  /** @override */
  attached() {
    this.diceWebSigninInterceptBrowserProxy_ =
        DiceWebSigninInterceptBrowserProxyImpl.getInstance();
    this.diceWebSigninInterceptBrowserProxy_.pageLoaded().then(
        info => this.handleAccountInfoChanged_(info));
    this.addWebUIListener(
        'account-info-changed', this.handleAccountInfoChanged_.bind(this));
  },

  /** @private */
  onAccept_() {
    this.diceWebSigninInterceptBrowserProxy_.accept();
  },

  /** @private */
  onCancel_() {
    this.diceWebSigninInterceptBrowserProxy_.cancel();
  },

  /**
   * Called when the account image changes.
   * @param {!AccountInfo} info
   * @private
   */
  handleAccountInfoChanged_(info) {
    this.accountInfo_ = info;
  },
});
