// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './strings.m.js';
import './signin_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SigninReauthBrowserProxy, SigninReauthBrowserProxyImpl} from './signin_reauth_browser_proxy.js';

Polymer({
  is: 'signin-reauth-app',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private */
    accountImageSrc_: {
      type: String,
      value() {
        return loadTimeData.getString('accountImageUrl');
      },
    },

    /** @private */
    confirmButtonLabel_: String,

    /** @private */
    confirmButtonHidden_: {type: Boolean, value: true},

    /** @private */
    cancelButtonHidden_: {type: Boolean, value: true}
  },

  /** @private {SigninReauthBrowserProxy} */
  signinReauthBrowserProxy_: null,

  /** @override */
  attached() {
    this.signinReauthBrowserProxy_ = SigninReauthBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'reauth-type-received', this.onReauthTypeReceived_.bind(this));
    this.signinReauthBrowserProxy_.initialize();
  },

  /** @private */
  onConfirm_() {
    this.signinReauthBrowserProxy_.confirm();
  },

  /** @private */
  onCancel_() {
    this.signinReauthBrowserProxy_.cancel();
  },

  /**
   * @param {boolean} requiresReauth Whether the user will be asked to
   *     reauthenticate after clicking on the confirm button.
   * @private
   */
  onReauthTypeReceived_(requiresReauth) {
    this.confirmButtonHidden_ = false;
    this.$.confirmButton.focus();
    this.cancelButtonHidden_ = requiresReauth;
    this.confirmButtonLabel_ = requiresReauth ?
        this.i18n('signinReauthNextLabel') :
        this.i18n('signinReauthConfirmLabel');
  },
});
