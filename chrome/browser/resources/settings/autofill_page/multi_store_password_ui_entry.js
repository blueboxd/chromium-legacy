// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStorePasswordUiEntry is used for showing entries that
 * are duplicated across stores as a single item in the UI.
 */

import {assert} from 'chrome://resources/js/assert.m.js';

import {MultiStoreIdHandler} from './multi_store_id_handler.js';
import {PasswordManagerProxy} from './password_manager_proxy.js';

/**
 * A version of PasswordManagerProxy.PasswordUiEntry used for deduplicating
 * entries from the device and the account.
 */
export class MultiStorePasswordUiEntry extends MultiStoreIdHandler {
  /**
   * Creates a multi-store item from duplicates |entry1| and (optional)
   * |entry2|. If both arguments are passed, they should have the same contents
   * but should be from different stores.
   * @param {!PasswordManagerProxy.PasswordUiEntry} entry1
   * @param {PasswordManagerProxy.PasswordUiEntry=} entry2
   */
  constructor(entry1, entry2) {
    super();

    /** @type {!MultiStorePasswordUiEntry.Contents} */
    this.contents_ = MultiStorePasswordUiEntry.getContents_(entry1);

    this.setId(entry1.id, entry1.fromAccountStore);

    if (entry2) {
      this.merge(entry2);
    }
  }

  /**
   * Incorporates the id of |otherEntry|, as long as |otherEntry| matches
   * |contents_| and the id corresponding to its store is not set.
   * @param {!PasswordManagerProxy.PasswordUiEntry} otherEntry
   */
  // TODO(crbug.com/1049141) Consider asserting frontendId as well.
  merge(otherEntry) {
    assert(
        (this.isPresentInAccount() && !otherEntry.fromAccountStore) ||
        (this.isPresentOnDevice() && otherEntry.fromAccountStore));
    assert(
        JSON.stringify(this.contents_) ===
        JSON.stringify(MultiStorePasswordUiEntry.getContents_(otherEntry)));
    this.setId(otherEntry.id, otherEntry.fromAccountStore);
  }

  get urls() {
    return this.contents_.urls;
  }
  get username() {
    return this.contents_.username;
  }
  get federationText() {
    return this.contents_.federationText;
  }

  /**
   * Extract all the information except for the id and fromPasswordStore.
   * @param {!PasswordManagerProxy.PasswordUiEntry} entry
   * @return {!MultiStorePasswordUiEntry.Contents}
   */
  static getContents_(entry) {
    return {
      urls: entry.urls,
      username: entry.username,
      federationText: entry.federationText
    };
  }
}

/**
 * @typedef {{
 *   urls: !PasswordManagerProxy.UrlCollection,
 *   username: string,
 *   federationText: (string|undefined)
 * }}
 */
MultiStorePasswordUiEntry.Contents;
