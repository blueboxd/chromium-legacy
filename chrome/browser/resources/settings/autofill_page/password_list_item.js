// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordListItem represents one row in the list of passwords.
 * It needs to be its own component because FocusRowBehavior provides good a11y.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import '../settings_shared_css.m.js';
import '../site_favicon.js';
import './passwords_shared_css.js';

import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {PasswordManagerProxy} from './password_manager_proxy.js';
import {ShowPasswordBehavior} from './show_password_behavior.js';

Polymer({
  is: 'password-list-item',

  _template: html`{__html_template__}`,

  behaviors: [
    FocusRowBehavior,
    ShowPasswordBehavior,
  ],

  properties: {isOptedInForAccountStorage: {type: Boolean, value: false}},

  /**
   * Selects the password on tap if revealed.
   * @private
   */
  onReadonlyInputTap_() {
    if (this.item.password) {
      this.$$('#password').select();
    }
  },

  /**
   * Opens the password action menu.
   * @private
   */
  onPasswordMenuTap_() {
    this.fire(
        'password-menu-tap', {target: this.$.passwordMenu, listItem: this});
  },

  /**
   * @private
   * @param {!PasswordManagerProxy.UiEntryWithPassword} item This row's item.
   * @return {string}
   */
  getStorageText_(item) {
    // TODO(crbug.com/1049141): Add proper translated strings once we have them.
    return item.entry.fromAccountStore ? 'Account' : 'Local';
  },

  /**
   * @private
   * @param {!PasswordManagerProxy.UiEntryWithPassword} item This row's item.
   * @return {string}
   */
  getStorageIcon_(item) {
    // TODO(crbug.com/1049141): Add the proper icons once we know them.
    return item.entry.fromAccountStore ? 'cr:sync' : 'cr:computer';
  },

  /**
   * Get the aria label for the More Actions button on this row.
   * @param {!PasswordManagerProxy.UiEntryWithPassword} item This row's item.
   * @private
   */
  getMoreActionsLabel_(item) {
    // Avoid using I18nBehavior.i18n, because it will filter sequences, which
    // are otherwise not illegal for usernames. Polymer still protects against
    // XSS injection.
    return loadTimeData.getStringF(
        (item.entry.federationText) ? 'passwordRowFederatedMoreActionsButton' :
                                      'passwordRowMoreActionsButton',
        item.entry.username, item.entry.urls.shown);
  },
});
