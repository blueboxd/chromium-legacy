// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.BitmapUriRequest;
import org.chromium.chrome.browser.share.BitmapUriRequestJni;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link ScreenshotShareSheetMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// clang-format off
@Features.EnableFeatures(ChromeFeatureList.CHROME_SHARE_SCREENSHOT)
public class ScreenshotShareSheetMediatorUnitTest {
    // clang-format on
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    Runnable mDeleteRunnable;

    @Mock
    Runnable mSaveRunnable;

    @Mock
    Activity mContext;

    @Mock
    Tab mTab;

    @Mock
    ChromeOptionShareCallback mShareCallback;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private BitmapUriRequest.Natives mBitmapUriRequest;

    private PropertyModel mModel;
    private ScreenshotShareSheetMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doNothing().when(mDeleteRunnable).run();

        doNothing().when(mSaveRunnable).run();

        doNothing().when(mShareCallback).showThirdPartyShareSheet(any(), any(), anyLong());

        mJniMocker.mock(BitmapUriRequestJni.TEST_HOOKS, mBitmapUriRequest);

        when(mBitmapUriRequest.bitmapUri(any())).thenReturn("bitmapUri");

        mModel = new PropertyModel(ScreenshotShareSheetViewProperties.ALL_KEYS);

        mMediator = new ScreenshotShareSheetMediator(
                mContext, mModel, mDeleteRunnable, mSaveRunnable, mTab, mShareCallback);
    }

    @Test
    public void onClickDelete() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.DELETE);

        verify(mDeleteRunnable).run();
    }

    @Test
    public void onClickSave() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.SAVE);

        verify(mSaveRunnable).run();
    }

    @Test
    public void onClickShare() {
        Callback<Integer> callback =
                mModel.get(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER);
        callback.onResult(ScreenshotShareSheetViewProperties.NoArgOperation.SHARE);

        verify(mShareCallback).showThirdPartyShareSheet(any(), any(), anyLong());
        verify(mDeleteRunnable).run();
    }

    @After
    public void tearDown() {}
}
