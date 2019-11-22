// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.hostimpl.storage;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.testing.conformance.storage.JournalStorageDirectConformanceTest;
import com.google.common.util.concurrent.MoreExecutors;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link PersistentContentStorage}. */
@RunWith(RobolectricTestRunner.class)
public class PersistentJournalStorageDirectTest extends JournalStorageDirectConformanceTest {
    private Context context;
    @Mock
    private ThreadUtils threadUtils;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = ApplicationProvider.getApplicationContext();
        journalStorage = new PersistentJournalStorage(
                context, MoreExecutors.directExecutor(), threadUtils, null);
    }

    @After
    public void tearDown() {
        context.getFilesDir().delete();
    }

    @Test
    public void sanitize_and_desanitize() {
        String[] reservedChars = {"|", "\\", "?", "*", "<", "\"", ":", ">"};
        for (String c : reservedChars) {
            String test = "test" + c;
            String sanitized = ((PersistentJournalStorage) journalStorage).sanitize(test);
            assertThat(sanitized.contains(c)).isFalse();
            String unsanitized = ((PersistentJournalStorage) journalStorage).desanitize(sanitized);
            assertThat(unsanitized).isEqualTo(test);
        }
    }
}
