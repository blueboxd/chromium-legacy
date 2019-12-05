// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.storage;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import com.google.common.util.concurrent.MoreExecutors;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.testing.conformance.storage.JournalStorageDirectConformanceTest;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link PersistentContentStorage}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PersistentJournalStorageDirectTest extends JournalStorageDirectConformanceTest {
    private Context mContext;
    @Mock
    private ThreadUtils mThreadUtils;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mJournalStorage = new PersistentJournalStorage(
                mContext, MoreExecutors.directExecutor(), mThreadUtils, null);
    }

    @After
    public void tearDown() {
        mContext.getFilesDir().delete();
    }

    @Test
    public void sanitize_and_desanitize() {
        String[] reservedChars = {"|", "\\", "?", "*", "<", "\"", ":", ">"};
        for (String c : reservedChars) {
            String test = "test" + c;
            String sanitized = ((PersistentJournalStorage) mJournalStorage).sanitize(test);
            assertThat(sanitized.contains(c)).isFalse();
            String unsanitized = ((PersistentJournalStorage) mJournalStorage).desanitize(sanitized);
            assertThat(unsanitized).isEqualTo(test);
        }
    }
}
