// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.storage.testing;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.feedstore.testing.DelegatingContentStorage;
import org.chromium.chrome.browser.feed.library.testing.conformance.storage.ContentStorageConformanceTest;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link InMemoryContentStorage}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InMemoryContentStorageTest extends ContentStorageConformanceTest {
    @Before
    public void setUp() {
        mStorage = new DelegatingContentStorage(new InMemoryContentStorage());
    }
}
