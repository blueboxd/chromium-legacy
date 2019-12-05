// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.storage.testing;

import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.feedstore.testing.DelegatingJournalStorage;
import org.chromium.chrome.browser.feed.library.testing.conformance.storage.JournalStorageConformanceTest;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link InMemoryJournalStorage}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InMemoryJournalStorageTest extends JournalStorageConformanceTest {
    @Before
    public void setUp() {
        initMocks(this);
        mJournalStorage = new DelegatingJournalStorage(new InMemoryJournalStorage());
    }
}
