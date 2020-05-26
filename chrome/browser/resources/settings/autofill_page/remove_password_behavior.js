// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

/**
 * This behavior bundles functionality required to remove a user password.
 *
 * @polymerBehavior
 */
export const RemovePasswordBehavior = {

  properties: {
    /**
     * The password that will be removed.
     * @type {!MultiStorePasswordUiEntry}
     */
    entry: Object,
  },

  /**
   * Removes all existing versions of the password.
   * @return {!RemovalResult}
   */
  // TODO(crbug.com/1049141): Expose APIs in the behavior to allow selecting
  // which versions to remove. Update the comment on this method then.
  requestRemove() {
    /** @type {!RemovalResult} */
    const result = {
      removedFromAccount: false,
      removedFromDevice: false,
    };

    /** @type {!Array<number>} */
    const idsToRemove = [];
    if (this.entry.isPresentInAccount()) {
      result.removedFromAccount = true;
      idsToRemove.push(this.entry.accountId);
    }
    if (this.entry.isPresentOnDevice()) {
      result.removedFromDevice = true;
      idsToRemove.push(this.entry.deviceId);
    }

    if (idsToRemove.length) {
      PasswordManagerImpl.getInstance().removeSavedPasswords(idsToRemove);
    }

    return result;
  },
};


/**
 * Used to inform which password versions were removed.
 * @typedef {{
 *   removedFromAccount: boolean,
 *   removedFromDevice: boolean,
 * }}
 */
export let RemovalResult;
