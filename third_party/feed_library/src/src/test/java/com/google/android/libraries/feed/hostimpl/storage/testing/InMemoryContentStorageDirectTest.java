// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.hostimpl.storage.testing;

import com.google.android.libraries.feed.testing.conformance.storage.ContentStorageDirectConformanceTest;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link InMemoryContentStorage}. */
@RunWith(RobolectricTestRunner.class)
public class InMemoryContentStorageDirectTest extends ContentStorageDirectConformanceTest {
    @Before
    public void setUp() {
        storage = new InMemoryContentStorage();
    }
}
