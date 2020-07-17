// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './strings.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiceWebSigninInterceptBrowserProxy, DiceWebSigninInterceptBrowserProxyImpl} from './dice_web_signin_intercept_browser_proxy.js';

Polymer({
  is: 'dice-web-signin-intercept-app',

  _template: html`{__html_template__}`,

  /** @private {?DiceWebSigninInterceptBrowserProxy} */
  diceWebSigninInterceptBrowserProxy_: null,

  /** @override */
  attached() {
    this.diceWebSigninInterceptBrowserProxy_ =
        DiceWebSigninInterceptBrowserProxyImpl.getInstance();
  },

  /** @private */
  onAccept_() {
    this.diceWebSigninInterceptBrowserProxy_.accept();
  },

  /** @private */
  onCancel_() {
    this.diceWebSigninInterceptBrowserProxy_.cancel();
  },
});
