// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './strings.m.js';
import './signin_shared_css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SigninReauthBrowserProxy, SigninReauthBrowserProxyImpl} from './signin_reauth_browser_proxy.js';

Polymer({
  is: 'signin-reauth-app',

  _template: html`{__html_template__}`,

  properties: {
    /** @private */
    accountImageSrc_: {
      type: String,
      value() {
        return loadTimeData.getString('accountImageUrl');
      },
    },
  },

  /** @private {SigninReauthBrowserProxy} */
  signinReauthBrowserProxy_: null,

  /** @override */
  attached() {
    this.signinReauthBrowserProxy_ = SigninReauthBrowserProxyImpl.getInstance();
  },

  /** @private */
  onConfirm_() {
    this.signinReauthBrowserProxy_.confirm();
  },

  /** @private */
  onCancel_() {
    this.signinReauthBrowserProxy_.cancel();
  },
});
