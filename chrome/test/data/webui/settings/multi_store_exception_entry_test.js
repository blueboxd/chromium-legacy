// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for MultiStoreExceptionEntry.
 */

import {MultiStoreExceptionEntry} from 'chrome://settings/settings.js';
import {createExceptionEntry} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';

suite('MultiStoreExceptionEntry', function() {
  test('verifyIds', function() {
    const deviceEntry = createExceptionEntry({url: 'g.com', id: 0});
    deviceEntry.fromAccountStore = false;
    const accountEntry = createExceptionEntry({url: 'g.com', id: 1});
    accountEntry.fromAccountStore = true;

    const multiStoreDeviceEntry = new MultiStoreExceptionEntry(deviceEntry);
    expectTrue(multiStoreDeviceEntry.isPresentOnDevice());
    expectFalse(multiStoreDeviceEntry.isPresentInAccount());
    expectEquals(multiStoreDeviceEntry.getAnyId(), 0);

    const multiStoreAccountEntry = new MultiStoreExceptionEntry(accountEntry);
    expectFalse(multiStoreAccountEntry.isPresentOnDevice());
    expectTrue(multiStoreAccountEntry.isPresentInAccount());
    expectEquals(multiStoreAccountEntry.getAnyId(), 1);

    const multiStoreEntryFromBoth =
        new MultiStoreExceptionEntry(deviceEntry, accountEntry);
    expectTrue(multiStoreEntryFromBoth.isPresentOnDevice());
    expectTrue(multiStoreEntryFromBoth.isPresentInAccount());
    expectTrue(
        multiStoreEntryFromBoth.getAnyId() === 0 ||
        multiStoreEntryFromBoth.getAnyId() === 1);
  });
});
