// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_login_css.js';
import './edu_login_template.js';
import './edu_login_button.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'edu-login-welcome',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * Whether current flow is a reauthentication.
   * @private {boolean}
   */
  reauthFlow_: false,

  /** @override */
  created() {
    // For the reauth flow the email is appended to query params in
    // InlineLoginHandlerDialogChromeOS. It's used later in auth extension to
    // pass the email value to Gaia.
    let currentQueryParameters = new URLSearchParams(window.location.search);
    if (currentQueryParameters.get('email')) {
      this.reauthFlow_ = true;
    }
    document.title = this.getTitle_();
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_() {
    return this.reauthFlow_ ? this.i18n('welcomeReauthTitle') :
                              this.i18n('welcomeTitle');
  },

  /**
   * @return {string}
   * @private
   */
  getBody_() {
    return this.reauthFlow_ ? this.i18n('welcomeReauthBody') :
                              this.i18n('welcomeBody');
  },
});
