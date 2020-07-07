// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';

Polymer({
  is: 'profile-card',

  _template: html`{__html_template__}`,

  properties: {
    /**  @type {!ProfileState} */
    profileState: {
      type: Object,
    }
  },

  /** @private {ManageProfilesBrowserProxy} */
  manageProfilesBrowserProxy_: null,

  /** @override */
  ready() {
    this.manageProfilesBrowserProxy_ =
        ManageProfilesBrowserProxyImpl.getInstance();
  },

  /** @private */
  onProfileClick_() {
    this.manageProfilesBrowserProxy_.openSelectedProfile(
        this.profileState.profilePath);
  },

});
