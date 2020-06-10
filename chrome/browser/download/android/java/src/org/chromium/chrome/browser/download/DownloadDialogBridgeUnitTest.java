// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.download.dialogs.DownloadLocationDialogCoordinator;
import org.chromium.ui.base.WindowAndroid;

/**
 * Unit test for {@link DownloadDialogBridge}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadDialogBridgeUnitTest {
    private static final int FAKE_NATIVE_HODLER = 1;
    private static final long TOTAL_BYTES = 100;
    private static final @DownloadLocationDialogType int LOCATION_DIALOG_TYPE =
            org.chromium.chrome.browser.download.DownloadLocationDialogType.DEFAULT;
    private static final String SUGGESTED_PATH = "sdcard/download.txt";

    private DownloadDialogBridge mBridge;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private DownloadDialogBridge.Natives mNativeMock;

    @Mock
    WindowAndroid mWindowAndroid;

    @Mock
    DownloadLocationDialogCoordinator mLocationDialog;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(DownloadDialogBridgeJni.TEST_HOOKS, mNativeMock);
        mBridge = new DownloadDialogBridge(FAKE_NATIVE_HODLER, mLocationDialog);
    }

    @After
    public void tearDown() {
        mBridge = null;
    }

    @Test
    public void testShowDialog() {
        mBridge.showDialog(mWindowAndroid, TOTAL_BYTES, LOCATION_DIALOG_TYPE, SUGGESTED_PATH);
        verify(mLocationDialog)
                .showDialog(eq(mWindowAndroid), eq(TOTAL_BYTES), eq(LOCATION_DIALOG_TYPE),
                        eq(SUGGESTED_PATH));
    }

    @Test
    public void testDestroy() {
        mBridge.destroy();
        verify(mLocationDialog).destroy();
    }

    @Test
    public void testLocationDialogComplete() {
        mBridge.onComplete(SUGGESTED_PATH);
        verify(mNativeMock).onComplete(anyLong(), any(), eq(SUGGESTED_PATH), eq(false), eq(-1L));
    }

    @Test
    public void testLocationDialogCanceled() {
        Assert.assertNotNull(mBridge);
        mBridge.onCancel();
        verify(mNativeMock).onCanceled(anyLong(), any());
    }
}
