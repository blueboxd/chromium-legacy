// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.MediumTest;
import android.webkit.ValueCallback;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.MediaCaptureCallback;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that Media Capture and Streams Web API (MediaStream) works as expected.
 */
@CommandLineFlags.Add({"ignore-certificate-errors"})
@RunWith(WebLayerJUnit4ClassRunner.class)
public final class MediaCaptureTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private TestWebLayer mTestWebLayer;
    private CallbackImpl mCaptureCallback;

    private static class CallbackImpl extends MediaCaptureCallback {
        boolean mAudio;
        boolean mVideo;
        public BoundedCountDownLatch mRequestedCountDown;
        public BoundedCountDownLatch mStateCountDown;

        @Override
        public void onMediaCaptureRequested(
                boolean audio, boolean video, ValueCallback<Boolean> requestResult) {
            requestResult.onReceiveValue(true);
            mRequestedCountDown.countDown();
        }

        @Override
        public void onMediaCaptureStateChanged(boolean audio, boolean video) {
            mAudio = audio;
            mVideo = video;
            mStateCountDown.countDown();
        }
    }

    @Before
    public void setUp() throws Throwable {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        Assert.assertNotNull(mActivity);

        mTestWebLayer = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
        mActivityTestRule.getTestServerRule().setServerUsesHttps(true);
        String testUrl =
                mActivityTestRule.getTestServer().getURL("/weblayer/test/data/getusermedia.html");

        mCaptureCallback = new CallbackImpl();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().getMediaCaptureController().setMediaCaptureCallback(
                    mCaptureCallback);
        });

        mActivityTestRule.navigateAndWait(testUrl);
    }

    /**
     * Basic test for a stream that includes audio and video.
     */
    @Test
    @MediumTest
    public void testMediaCapture_basic() throws Throwable {
        CriteriaHelper.pollInstrumentationThread(
                () -> { return mTestWebLayer.isPermissionDialogShown(); });
        mCaptureCallback.mRequestedCountDown = new BoundedCountDownLatch(1);
        mCaptureCallback.mStateCountDown = new BoundedCountDownLatch(1);
        mTestWebLayer.clickPermissionDialogButton(true);

        mCaptureCallback.mRequestedCountDown.timedAwait();
        mCaptureCallback.mStateCountDown.timedAwait();
        Assert.assertTrue(mCaptureCallback.mAudio);
        Assert.assertTrue(mCaptureCallback.mVideo);

        mCaptureCallback.mStateCountDown = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().getMediaCaptureController().stopMediaCapturing(); });
        mCaptureCallback.mStateCountDown.timedAwait();
        Assert.assertFalse(mCaptureCallback.mAudio);
        Assert.assertFalse(mCaptureCallback.mVideo);
    }
}
