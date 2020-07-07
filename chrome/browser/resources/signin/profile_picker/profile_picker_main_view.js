// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './icons.js';
import './profile_card.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';
import {navigateTo, NavigationBehavior, Routes} from './navigation_behavior.js';


Polymer({
  is: 'profile-picker-main-view',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior, NavigationBehavior],

  properties: {
    /**
     * Profiles list supplied by ManageProfilesBrowserProxy.
     * @type {!Array<!ProfileState>}
     */
    profilesList_: {
      type: Object,
    },
  },

  /** @private {?ManageProfilesBrowserProxy} */
  manageProfilesBrowserProxy_: null,

  /** @override */
  ready() {
    this.manageProfilesBrowserProxy_ =
        ManageProfilesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'profiles-list-changed', this.handleProfilesListChanged_.bind(this));
    this.manageProfilesBrowserProxy_.initializeMainView();
  },

  /**
   * Handler for when the profiles list are updated.
   * @param {!Array<!ProfileState>} profilesList
   * @private
   */
  handleProfilesListChanged_(profilesList) {
    this.profilesList_ = profilesList;
  },

  /** @private */
  onAddProfileClick_() {
    navigateTo(Routes.NEW_PROFILE);
  },
});
