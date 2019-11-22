// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.hostimpl.storage;

import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.testing.conformance.storage.ContentStorageConformanceTest;
import com.google.common.util.concurrent.MoreExecutors;

import org.junit.After;
import org.junit.Before;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link PersistentContentStorage}. */
@RunWith(RobolectricTestRunner.class)
public class PersistentContentStorageTest extends ContentStorageConformanceTest {
    @Mock
    private ThreadUtils threadUtils;
    private Context context;

    @Before
    public void setUp() {
        initMocks(this);
        context = ApplicationProvider.getApplicationContext();
        storage = new PersistentContentStorage(
                context, MoreExecutors.newDirectExecutorService(), threadUtils);
    }

    @After
    public void tearDown() {
        context.getFilesDir().delete();
    }
}
