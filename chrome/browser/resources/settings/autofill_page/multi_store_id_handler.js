// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common code used by MultiStorePasswordUiEntry and
 * MultiStoreIdHandler to deal with ids from different stores.
 */

import {assertNotReached} from 'chrome://resources/js/assert.m.js';

export class MultiStoreIdHandler {
  constructor() {
    /** @type {number?} */
    this.deviceId_ = null;
    /** @type {number?} */
    this.accountId_ = null;
  }

  /**
   * Get any of the present ids. At least one of the ids must have been set
   * before this method is invoked.
   * @return {number}
   */
  getAnyId() {
    if (this.deviceId_ !== null) {
      return this.deviceId_;
    }
    if (this.accountId_ !== null) {
      return this.accountId_;
    }
    assertNotReached();
    return 0;
  }

  /**
   * Whether one of the ids is from the account.
   * @return {boolean}
   */
  isPresentInAccount() {
    return this.accountId_ !== null;
  }

  /**
   * Whether one of the ids is from the device.
   * @return {boolean}
   */
  isPresentOnDevice() {
    return this.deviceId_ !== null;
  }

  get deviceId() {
    return this.deviceId_;
  }
  get accountId() {
    return this.accountId_;
  }

  /**
   * @protected
   * @param {number} id
   * @param {boolean} fromAccountStore
   */
  setId(id, fromAccountStore) {
    if (fromAccountStore) {
      this.accountId_ = id;
    } else {
      this.deviceId_ = id;
    }
  }
}
