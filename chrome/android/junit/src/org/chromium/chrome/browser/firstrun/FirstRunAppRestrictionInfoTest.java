// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.os.Bundle;
import android.os.UserManager;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.testing.local.CustomShadowUserManager;

import java.util.concurrent.TimeoutException;

/**
 * Unit test for {@link FirstRunAppRestrictionInfo}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowRecordHistogram.class, ShadowPostTask.class,
                CustomShadowUserManager.class})
public class FirstRunAppRestrictionInfoTest {
    private static final String FRE_HISTOGRAM = "Enterprise.FirstRun.AppRestrictionLoadTime";

    private static class BooleanInputCallbackHelper extends CallbackHelper {
        boolean mLastInput;

        void notifyCalledWithInput(boolean input) {
            mLastInput = input;
            notifyCalled();
        }

        void assertCallbackHelperCalledWithInput(boolean expected) throws TimeoutException {
            waitForFirst();
            Assert.assertEquals("Callback helper should be called once.", 1, getCallCount());
            Assert.assertEquals(expected, mLastInput);
        }
    }

    private FirstRunAppRestrictionInfo mAppRestrictionInfo = new FirstRunAppRestrictionInfo(null);

    @Mock
    private Bundle mMockBundle;
    private boolean mPauseDuringPostTask;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        ShadowRecordHistogram.reset();
        ShadowPostTask.setTestImpl(new ShadowPostTask.TestImpl() {
            @Override
            public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
                if (!mPauseDuringPostTask) task.run();
            }
        });

        Context context = ContextUtils.getApplicationContext();
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        CustomShadowUserManager shadowUserManager = (CustomShadowUserManager) shadowOf(userManager);
        shadowUserManager.setApplicationRestrictions(context.getPackageName(), mMockBundle);
    }

    @Test
    @SmallTest
    public void testInitWithRestriction() throws TimeoutException {
        testInitImpl(true);
    }

    @Test
    @SmallTest
    public void testInitWithoutRestriction() throws TimeoutException {
        testInitImpl(false);
    }

    private void testInitImpl(boolean withRestriction) throws TimeoutException {
        Mockito.when(mMockBundle.isEmpty()).thenReturn(!withRestriction);

        TestThreadUtils.runOnUiThreadBlocking(() -> mAppRestrictionInfo.initialize());

        final BooleanInputCallbackHelper callbackHelper = new BooleanInputCallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mAppRestrictionInfo.getHasAppRestriction(
                                callbackHelper::notifyCalledWithInput));
        callbackHelper.assertCallbackHelperCalledWithInput(withRestriction);

        Assert.assertEquals("Histogram should be recorded once.", 1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(FRE_HISTOGRAM));
    }

    @Test
    @SmallTest
    public void testQueuedCallback() throws TimeoutException {
        Mockito.when(mMockBundle.isEmpty()).thenReturn(false);

        final BooleanInputCallbackHelper callbackHelper1 = new BooleanInputCallbackHelper();
        final BooleanInputCallbackHelper callbackHelper2 = new BooleanInputCallbackHelper();
        final BooleanInputCallbackHelper callbackHelper3 = new BooleanInputCallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAppRestrictionInfo.getHasAppRestriction(callbackHelper1::notifyCalledWithInput);
            mAppRestrictionInfo.getHasAppRestriction(callbackHelper2::notifyCalledWithInput);
            mAppRestrictionInfo.getHasAppRestriction(callbackHelper3::notifyCalledWithInput);
        });

        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, callbackHelper1.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, callbackHelper2.getCallCount());
        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, callbackHelper3.getCallCount());

        // Initialized the AppRestrictionInfo and wait until initialized.
        TestThreadUtils.runOnUiThreadBlocking(() -> mAppRestrictionInfo.initialize());

        callbackHelper1.assertCallbackHelperCalledWithInput(true);
        callbackHelper2.assertCallbackHelperCalledWithInput(true);
        callbackHelper3.assertCallbackHelperCalledWithInput(true);

        Assert.assertEquals("Histogram should be recorded once.", 1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(FRE_HISTOGRAM));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        final BooleanInputCallbackHelper callbackHelper = new BooleanInputCallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mAppRestrictionInfo.getHasAppRestriction(
                                callbackHelper::notifyCalledWithInput));

        // Destroy the object before initialization.
        mPauseDuringPostTask = true;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAppRestrictionInfo.initialize();
            FirstRunAppRestrictionInfo.destroy();
        });

        Assert.assertEquals(
                "CallbackHelper should not triggered yet.", 0, callbackHelper.getCallCount());
        Assert.assertEquals("Histogram should not be recorded as task is canceled.", 0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(FRE_HISTOGRAM));
    }
}
