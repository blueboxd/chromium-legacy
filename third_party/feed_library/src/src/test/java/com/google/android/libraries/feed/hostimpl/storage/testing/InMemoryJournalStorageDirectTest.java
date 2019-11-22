// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.hostimpl.storage.testing;

import com.google.android.libraries.feed.testing.conformance.storage.JournalStorageDirectConformanceTest;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link InMemoryJournalStorage}. */
@RunWith(RobolectricTestRunner.class)
public class InMemoryJournalStorageDirectTest extends JournalStorageDirectConformanceTest {
    @Before
    public void setUp() {
        journalStorage = new InMemoryJournalStorage();
    }
}
