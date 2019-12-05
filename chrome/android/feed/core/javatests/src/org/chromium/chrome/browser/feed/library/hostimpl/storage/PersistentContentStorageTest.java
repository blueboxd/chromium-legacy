// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.storage;

import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import com.google.common.util.concurrent.MoreExecutors;

import org.junit.After;
import org.junit.Before;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.testing.conformance.storage.ContentStorageConformanceTest;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link PersistentContentStorage}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PersistentContentStorageTest extends ContentStorageConformanceTest {
    @Mock
    private ThreadUtils mThreadUtils;
    private Context mContext;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mStorage = new PersistentContentStorage(
                mContext, MoreExecutors.newDirectExecutorService(), mThreadUtils);
    }

    @After
    public void tearDown() {
        mContext.getFilesDir().delete();
    }
}
