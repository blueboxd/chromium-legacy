// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStoreExceptionEntry is used for showing exceptions that
 * are duplicated across stores as a single item in the UI.
 */

import {assert} from 'chrome://resources/js/assert.m.js';

import {MultiStoreIdHandler} from './multi_store_id_handler.js';
import {PasswordManagerProxy} from './password_manager_proxy.js';

/**
 * A version of PasswordManagerProxy.ExceptionEntry used for deduplicating
 * exceptions from the device and the account.
 */
export class MultiStoreExceptionEntry extends MultiStoreIdHandler {
  /**
   * Creates a multi-store entry from duplicates |entry1| and (optional)
   * |entry2|. If both arguments are passed, they should have the same contents
   * but should be from different stores.
   * @param {!PasswordManagerProxy.ExceptionEntry} entry1
   * @param {PasswordManagerProxy.ExceptionEntry=} entry2
   */
  constructor(entry1, entry2) {
    super();

    /** @type {!PasswordManagerProxy.UrlCollection} */
    this.urls_ = entry1.urls;

    this.setId(entry1.id, entry1.fromAccountStore);

    if (entry2) {
      this.merge(entry2);
    }
  }

  /**
   * Incorporates the id of |otherEntry|, as long as |otherEntry| matches
   * |contents_| and the id corresponding to its store is not set.
   * @param {!PasswordManagerProxy.ExceptionEntry} otherEntry
   */
  // TODO(crbug.com/1102294) Consider asserting frontendId as well.
  merge(otherEntry) {
    assert(
        (this.isPresentInAccount() && !otherEntry.fromAccountStore) ||
        (this.isPresentOnDevice() && otherEntry.fromAccountStore));
    assert(JSON.stringify(this.urls_) === JSON.stringify(otherEntry.urls));
    this.setId(otherEntry.id, otherEntry.fromAccountStore);
  }

  get urls() {
    return this.urls_;
  }
}
