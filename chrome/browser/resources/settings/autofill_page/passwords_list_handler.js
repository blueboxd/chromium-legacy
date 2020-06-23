// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordsListHandler is a container for a passwords list
 * responsible for handling events associated with the overflow menu (copy,
 * editing, removal, moving to account).
 */

import './password_edit_dialog.js';
import './password_move_to_account_dialog.js';
import './password_remove_dialog.js';
import './password_list_item.js';
import './password_edit_dialog.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

// <if expr="chromeos">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {PasswordMoreActionsClickedEvent} from './password_list_item.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';

Polymer({
  is: 'passwords-list-handler',

  _template: html`{__html_template__}`,

  properties: {

    /**
     * The model for any active menus or dialogs. The value is reset to null
     * whenever actions from the menus/dialogs are concluded.
     * @private {?PasswordListItemElement}
     */
    activePassword: {
      type: Object,
      value: null,
    },

    /**
     * Whether the edit dialog and removal notification should show information
     * about which location(s) a password is stored.
     */
    shouldShowStorageDetails: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether an option for moving a password to the account should be offered
     * in the overflow menu.
     */
    shouldShowMoveToAccountOption: {
      type: Boolean,
      value: false,
    },

    // <if expr="chromeos">
    /** @type {BlockingRequestManager} */
    tokenRequestManager: Object,
    // </if>

    /** @private */
    enablePasswordCheck_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enablePasswordCheck');
      }
    },

    /** @private */
    showPasswordEditDialog_: {type: Boolean, value: false},

    /** @private */
    showPasswordMoveToAccountDialog_: {type: Boolean, value: false},

    /** @private */
    showPasswordRemoveDialog_: {type: Boolean, value: false},

    /**
     * The element to return focus to, when the currently active dialog is
     * closed.
     * @private {?HTMLElement}
     */
    activeDialogAnchor_: {type: Object, value: null},


    /**
     * The message displayed in the toast following a password removal.
     */
    removalNotification_: {
      type: String,
      value: '',
    }
  },

  behaviors: [
    I18nBehavior,
  ],

  listeners: {
    'password-more-actions-clicked': 'onPasswordMoreActionsClicked_',
    'password-remove-dialog-passwords-removed':
        'onPasswordRemoveDialogPasswordsRemoved_',
  },

  /** @override */
  detached() {
    if (this.$.toast.open) {
      this.$.toast.hide();
    }
  },

  /**
   * Closes the toast manager.
   */
  onSavedPasswordOrExceptionRemoved() {
    this.$.toast.hide();
  },

  /**
   * Opens the password action menu.
   * @param {PasswordMoreActionsClickedEvent} event
   * @private
   */
  onPasswordMoreActionsClicked_(event) {
    const target = event.detail.target;

    this.activePassword = event.detail.listItem;
    this.$.menu.showAt(target);
    this.activeDialogAnchor_ = target;
  },

  /**
   * Shows the edit password dialog.
   * @param {!Event} e
   * @private
   */
  onMenuEditPasswordTap_(e) {
    e.preventDefault();
    this.$.menu.close();
    this.showPasswordEditDialog_ = true;
  },

  /** @private */
  onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
    this.activePassword = null;
  },

  /** @private */
  onMovePasswordToAccountDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
    this.activePassword = null;
  },

  /**
   * Copy selected password to clipboard.
   * @private
   */
  onMenuCopyPasswordButtonTap_() {
    // Copy to clipboard occurs inside C++ and we don't expect getting
    // result back to javascript.
    PasswordManagerImpl.getInstance()
        .requestPlaintextPassword(
            this.activePassword.entry.getAnyId(),
            chrome.passwordsPrivate.PlaintextReason.COPY)
        .then(_ => {
          this.activePassword = null;
        })
        .catch(error => {
          // <if expr="chromeos">
          // If no password was found, refresh auth token and retry.
          this.tokenRequestManager.request(
              this.onMenuCopyPasswordButtonTap_.bind(this));
          // </if>});
        });
    this.$.menu.close();
  },

  /**
   * Handler for the remove option in the overflow menu. If the password only
   * exists in one location, deletes it directly. Otherwise, opens the remove
   * dialog to allow choosing from which locations to remove.
   * @private
   */
  onMenuRemovePasswordTap_() {
    this.$.menu.close();

    if (this.activePassword.entry.isPresentOnDevice() &&
        this.activePassword.entry.isPresentInAccount()) {
      this.showPasswordRemoveDialog_ = true;
      return;
    }

    const idToRemove = this.activePassword.entry.isPresentInAccount() ?
        this.activePassword.entry.accountId :
        this.activePassword.entry.deviceId;
    PasswordManagerImpl.getInstance().removeSavedPassword(idToRemove);
    this.displayRemovalNotification_(
        this.activePassword.entry.isPresentInAccount(),
        this.activePassword.entry.isPresentOnDevice());
    this.activePassword = null;
  },

  /**
   * At least one of |removedFromAccount| or |removedFromDevice| must be true.
   * @param {boolean} removedFromAccount
   * @param {boolean} removedFromDevice
   * @private
   */
  displayRemovalNotification_(removedFromAccount, removedFromDevice) {
    assert(removedFromAccount || removedFromDevice);
    this.removalNotification_ = this.i18n('passwordDeleted');
    if (this.shouldShowStorageDetails) {
      if (removedFromAccount && removedFromDevice) {
        this.removalNotification_ =
            this.i18n('passwordDeletedFromAccountAndDevice');
      } else if (removedFromAccount) {
        this.removalNotification_ = this.i18n('passwordDeletedFromAccount');
      } else {
        this.removalNotification_ = this.i18n('passwordDeletedFromDevice');
      }
    }
    this.$.toast.show();
    this.fire('iron-announce', {text: this.removalNotification_});
    this.fire('iron-announce', {text: this.i18n('undoDescription')});
  },

  /**
   * @param {PasswordRemoveDialogPasswordsRemovedEvent} event
   */
  onPasswordRemoveDialogPasswordsRemoved_(event) {
    this.displayRemovalNotification_(
        event.detail.removedFromAccount, event.detail.removedFromDevice);
  },

  /**
   * @private
   */
  onUndoButtonClick_() {
    PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
    this.onSavedPasswordOrExceptionRemoved();
  },

  /**
   * Should only be called when |activePassword| has a device copy.
   * @param {!Event} event
   * @private
   */
  onMenuMovePasswordToAccountTap_(event) {
    event.preventDefault();
    this.$.menu.close();
    this.showPasswordMoveToAccountDialog_ = true;
  },

  /**
   * @private
   */
  onPasswordMoveToAccountDialogClosed_() {
    this.showPasswordMoveToAccountDialog_ = false;
    this.activePassword = null;

    // The entry possibly disappeared, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  },

  /**
   * @private
   */
  onPasswordRemoveDialogClosed_() {
    this.showPasswordRemoveDialog_ = false;
    this.activePassword = null;

    // A removal possibly happened, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  },

});
