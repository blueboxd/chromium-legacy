// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link LoadingView}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowView.class})
public class LoadingViewTest {
    private LoadingView mLoadingView;
    private Activity mActivity;

    private final CallbackHelper mCallback1 = new CallbackHelper();
    private final CallbackHelper mCallback2 = new CallbackHelper();

    @Before
    public void setUpTest() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        FrameLayout content = new FrameLayout(mActivity);
        mActivity.setContentView(content);

        mLoadingView = new LoadingView(mActivity);
        mLoadingView.setDisableAnimationForTest(true);
        content.addView(mLoadingView);

        mLoadingView.addObserver(mCallback1::notifyCalled);
        mLoadingView.addObserver(mCallback2::notifyCalled);
    }

    @Test
    @SmallTest
    public void testLoadingFast() {
        mLoadingView.showLoadingUI();

        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        Assert.assertEquals("Progress bar should be hidden before 500ms.", View.GONE,
                mLoadingView.getVisibility());

        mLoadingView.hideLoadingUI();
        Assert.assertEquals(
                "Progress bar should never be visible.", View.GONE, mLoadingView.getVisibility());
        Assert.assertEquals("Callback1 should be executed after loading finishes.", 1,
                mCallback1.getCallCount());
        Assert.assertEquals("Callback2 should be executed after loading finishes.", 1,
                mCallback2.getCallCount());
    }

    @Test
    @SmallTest
    public void testLoadingSlow() {
        long sleepTime = 500;
        mLoadingView.showLoadingUI();

        ShadowLooper.idleMainLooper(sleepTime, TimeUnit.MILLISECONDS);
        Assert.assertEquals("Progress bar should be visible after 500ms.", View.VISIBLE,
                mLoadingView.getVisibility());

        mLoadingView.hideLoadingUI();
        Assert.assertEquals("Progress bar should still be visible until showing for 500ms.",
                View.VISIBLE, mLoadingView.getVisibility());
        Assert.assertEquals("Callback1 should not be executed before loading finishes.", 0,
                mCallback1.getCallCount());
        Assert.assertEquals("Callback2 should not be executed before loading finishes.", 0,
                mCallback2.getCallCount());

        // The spinner should be displayed for at least 500ms.
        ShadowLooper.idleMainLooper(sleepTime, TimeUnit.MILLISECONDS);
        Assert.assertEquals("Progress bar should be hidden after 500ms.", View.GONE,
                mLoadingView.getVisibility());
        Assert.assertEquals("Callback1 should be executed after loading finishes.", 1,
                mCallback1.getCallCount());
        Assert.assertEquals("Callback2 should be executed after loading finishes.", 1,
                mCallback2.getCallCount());
    }
}
