// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for MultiStorePasswordUiEntry.
 */

import {MultiStorePasswordUiEntry, PasswordManagerProxy} from 'chrome://settings/settings.js';
import {createPasswordEntry} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';

suite('MultiStorePasswordUiEntry', function() {
  test('verifyIds', function() {
    const deviceEntry = createPasswordEntry('g.com', 'user', 0);
    deviceEntry.fromAccountStore = false;
    const accountEntry = createPasswordEntry('g.com', 'user', 1);
    accountEntry.fromAccountStore = true;

    const multiStoreDeviceEntry = new MultiStorePasswordUiEntry(deviceEntry);
    expectTrue(multiStoreDeviceEntry.isPresentOnDevice());
    expectFalse(multiStoreDeviceEntry.isPresentInAccount());
    expectEquals(multiStoreDeviceEntry.getAnyId(), deviceEntry.id);

    const multiStoreAccountEntry = new MultiStorePasswordUiEntry(accountEntry);
    expectFalse(multiStoreAccountEntry.isPresentOnDevice());
    expectTrue(multiStoreAccountEntry.isPresentInAccount());
    expectEquals(multiStoreAccountEntry.getAnyId(), accountEntry.id);

    const multiStoreEntryFromBoth =
        new MultiStorePasswordUiEntry(deviceEntry, accountEntry);
    expectTrue(multiStoreEntryFromBoth.isPresentOnDevice());
    expectTrue(multiStoreEntryFromBoth.isPresentInAccount());
    expectTrue(
        multiStoreEntryFromBoth.getAnyId() === deviceEntry.id ||
        multiStoreEntryFromBoth.getAnyId() === accountEntry.id);
  });
});
