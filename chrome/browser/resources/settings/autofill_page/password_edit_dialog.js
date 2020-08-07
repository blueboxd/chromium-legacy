// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-edit-dialog' is the dialog that allows showing a
 *     saved password.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../icons.m.js';
import '../settings_shared_css.m.js';
import '../settings_vars_css.m.js';
import './passwords_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {ShowPasswordBehavior} from './show_password_behavior.js';

Polymer({
  is: 'password-edit-dialog',

  _template: html`{__html_template__}`,

  behaviors: [ShowPasswordBehavior, I18nBehavior],

  properties: {
    shouldShowStorageDetails: {type: Boolean, value: false},

    /** @private */
    editPasswordsInSettings_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('editPasswordsInSettings');
      }
    },

    /**
     * Check if editPasswordsInSettings flag is true and entry isn't federation
     * credential.
     * @private
     * */
    isEditDialog_: {
      type: Boolean,
      computed: 'computeIsEditDialog_(editPasswordsInSettings_, entry)'
    },

    /**
     * Whether the input is invalid.
     * @private
     */
    inputInvalid_: Boolean,
  },

  /** @private {?PasswordManagerProxy} */
  passwordManager_: null,

  /** @override */
  attached() {
    this.passwordManager_ = PasswordManagerImpl.getInstance();
    this.$.dialog.showModal();
  },

  /**
   * Helper function that checks if editPasswordsInSettings flag is true and
   * entry isn't federation credential.
   * @return {boolean}
   * @private
   * */
  computeIsEditDialog_() {
    return this.editPasswordsInSettings_ && !this.entry.federationText;
  },

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  },

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancel_() {
    this.close();
  },

  /**
   * Handler for tapping the 'done' or 'save' button depending on isEditDialog_.
   * For 'save' button it should save new password. After pressing action button
   * button the edit dialog should be closed.
   * @private
   */
  onActionButtonTap_() {
    if (this.isEditDialog_) {
      const idsToChange = [];
      const accountId = this.entry.accountId;
      const deviceId = this.entry.deviceId;
      if (accountId !== null) {
        idsToChange.push(accountId);
      }
      if (deviceId !== null) {
        idsToChange.push(deviceId);
      }

      this.passwordManager_
          .changeSavedPassword(idsToChange, this.$.passwordInput.value)
          .finally(() => {
            this.close();
          });
    } else {
      this.close();
    }
  },

  /**
   * @return {string}
   * @private
   */
  getActionButtonName_() {
    return this.isEditDialog_ ? this.i18n('save') : this.i18n('done');
  },

  /** Manually de-select texts for readonly inputs. */
  onInputBlur_() {
    this.shadowRoot.getSelection().removeAllRanges();
  },

  /**
   * Gets the HTML-formatted message to indicate in which locations the password
   * is stored.
   */
  getStorageDetailsMessage_() {
    if (this.entry.isPresentInAccount() && this.entry.isPresentOnDevice()) {
      return this.i18n('passwordStoredInAccountAndOnDevice');
    }
    return this.entry.isPresentInAccount() ?
        this.i18n('passwordStoredInAccount') :
        this.i18n('passwordStoredOnDevice');
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_() {
    return this.isEditDialog_ ? this.i18n('editPasswordTitle') :
                                this.i18n('passwordDetailsTitle');
  },

  /**
   * @return {string} The text to be displayed as the dialog's footnote.
   * @private
   */
  getFootnote_() {
    return this.i18n('editPasswordFootnote', this.entry.urls.shown);
  }
});
