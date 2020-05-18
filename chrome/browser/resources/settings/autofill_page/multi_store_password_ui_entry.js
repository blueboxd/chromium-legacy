// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStorePasswordUiEntry is used for showing entries that
 * are duplicated across stores as a single item in the UI.
 */

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';

import {PasswordManagerProxy} from './password_manager_proxy.js';

/**
 * A version of PasswordManagerProxy.PasswordUiEntry used for deduplicating
 * entries from the device and the account.
 */
export class MultiStorePasswordUiEntry {
  /**
   * Creates a multi-store item from duplicates |entry1| and (optional)
   * |entry2|. If both arguments are passed, they should have the same contents
   * but should be from different stores.
   * @param {!PasswordManagerProxy.PasswordUiEntry} entry1
   * @param {PasswordManagerProxy.PasswordUiEntry=} entry2
   */
  constructor(entry1, entry2) {
    /** @type {!MultiStorePasswordUiEntry.Contents} */
    this.contents_ = MultiStorePasswordUiEntry.getContents_(entry1);
    /** @type {number?} */
    this.deviceId_ = null;
    /** @type {number?} */
    this.accountId_ = null;

    this.setId_(entry1.id, entry1.fromAccountStore);

    if (entry2) {
      assert(
        JSON.stringify(this.contents_) ===
        JSON.stringify(MultiStorePasswordUiEntry.getContents_(entry2)));
      assert(entry1.fromAccountStore !== entry2.fromAccountStore);
      this.setId_(entry2.id, entry2.fromAccountStore);
    }
  }

  /**
   * Get any of the present ids. The return value is guaranteed to be non-null.
   * @return {number}
   */
  getAnyId() {
    if (this.deviceId_ !== null) {
      return this.deviceId_;
    }
    if (this.accountId_ !== null) {
      return this.accountId_;
    }
    // One of the ids is guaranteed to be non-null by the constructor.
    assertNotReached();
    return 0;
  }

  /**
   * Whether one of the duplicates is from the account.
   * @return {boolean}
   */
  isPresentInAccount() {
    return this.accountId_ !== null;
  }

  /**
   * Whether one of the duplicates is from the device.
   * @return {boolean}
   */
  isPresentOnDevice() {
    return this.deviceId_ !== null;
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
  get deviceId() {
    return this.deviceId_;
  }
  get accountId() {
    return this.accountId_;
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

  /**
   * @param {number} id
   * @param {boolean} fromAccountStore
   */
  setId_(id, fromAccountStore) {
    if (fromAccountStore) {
      this.accountId_ = id;
    } else {
      this.deviceId_ = id;
    }
  }
}

/**
 * @typedef {{
 * urls: !PasswordManagerProxy.UrlCollection,
 * username: string,
 * federationText: (string|undefined)
 * }}
 */
MultiStorePasswordUiEntry.Contents;

/**
 * @typedef {{ entry: !MultiStorePasswordUiEntry, password: string }}
 */
export let MultiStorePasswordUiEntryWithPassword;
