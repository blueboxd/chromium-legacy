// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.io.File;

/**
 * Tests relating to  {@link FilePersistedTabDataStorage}
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class FilePersistedTabDataStorageTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock
    private Callback<byte[]> mByteArrayCallback;

    private static final int TAB_ID = 1;
    private static final byte[] DATA = {13, 14};
    private static final String DATA_ID = "DataId";

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    @SmallTest
    @Test
    public void testFilePersistedDataStorageNonEncrypted() {
        testFilePersistedDataStorage(false);
    }

    @SmallTest
    @Test
    public void testFilePersistedDataStorageEncrypted() {
        testFilePersistedDataStorage(true);
    }

    private void testFilePersistedDataStorage(boolean isEncrypted) {
        FilePersistedTabDataStorage filePersistedTabDataStorage = new FilePersistedTabDataStorage();
        filePersistedTabDataStorage.save(TAB_ID, isEncrypted, DATA_ID, DATA);
        ArgumentCaptor<byte[]> byteArrayCaptor = ArgumentCaptor.forClass(byte[].class);
        filePersistedTabDataStorage.restore(TAB_ID, isEncrypted, DATA_ID, mByteArrayCallback);
        verify(mByteArrayCallback, times(1)).onResult(byteArrayCaptor.capture());
        Assert.assertEquals(byteArrayCaptor.getValue().length, 2);
        Assert.assertArrayEquals(byteArrayCaptor.getValue(), DATA);
        File file = filePersistedTabDataStorage.getFile(TAB_ID, isEncrypted, DATA_ID);
        Assert.assertTrue(file.exists());
        filePersistedTabDataStorage.delete(TAB_ID, isEncrypted, DATA_ID);
        Assert.assertFalse(file.exists());
    }
}
