// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-move-to-account-dialog' is the dialog that allows
 * moving a password stored on the user device to the account.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';


Polymer({
  is: 'password-move-to-account-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {!MultiStorePasswordUiEntry}
     */
    passwordToMove: Object,

  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /**
   * @private
   */
  onMoveButtonClick_() {
    assert(this.passwordToMove.isPresentOnDevice());
    PasswordManagerImpl.getInstance()
        .movePasswordToAccount(/** @type {number} */
                               (this.passwordToMove.deviceId));
    this.$.dialog.close();
  },

  /**
   * @private
   */
  onCancelButtonClick_() {
    this.$.dialog.close();
  }
});
