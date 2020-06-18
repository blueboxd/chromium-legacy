// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

    private PropertyModel mModel;
    private ScreenshotShareSheetMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doNothing().when(mDeleteRunnable).run();
        doNothing().when(mSaveRunnable).run();

        mModel = new PropertyModel(ScreenshotShareSheetViewProperties.ALL_KEYS);

        mMediator =
                new ScreenshotShareSheetMediator(mContext, mModel, mDeleteRunnable, mSaveRunnable);
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

    @After
    public void tearDown() {}
}
