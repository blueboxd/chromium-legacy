// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.weblayer.FullscreenCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that FullscreenCallback methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class FullscreenCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private InstrumentationActivity mActivity;
    private Delegate mDelegate;

    private static class Delegate extends FullscreenCallback {
        public int mEnterFullscreenCount;
        public int mExitFullscreenCount;
        public Runnable mExitFullscreenRunnable;

        @Override
        public void enterFullscreen(Runnable exitFullscreenRunner) {
            mEnterFullscreenCount++;
            mExitFullscreenRunnable = exitFullscreenRunner;
        }

        @Override
        public void exitFullscreen() {
            mExitFullscreenCount++;
        }

        public void waitForFullscreen() {
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mEnterFullscreenCount == 1;
                }
            });
        }

        public void waitForExitFullscreen() {
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mExitFullscreenCount == 1;
                }
            });
        }
    }

    @Before
    public void setUp() {
        mTestServer = new EmbeddedTestServer();
        mTestServer.initializeNative(InstrumentationRegistry.getInstrumentation().getContext(),
                EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mTestServer.addDefaultHandlers("weblayer/test/data");
        Assert.assertTrue(mTestServer.start(0));

        String url = mTestServer.getURL("/fullscreen.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        mDelegate = new Delegate();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowserController().setFullscreenCallback(mDelegate); });

        // First touch enters fullscreen.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForFullscreen();
        Assert.assertEquals(1, mDelegate.mEnterFullscreenCount);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    public void testFullscreen() {
        // Second touch exits.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }

    @Test
    @SmallTest
    public void testExitFullscreenWhenDelegateCleared() {
        // Clearing the FullscreenCallback should exit fullscreen.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowserController().setFullscreenCallback(null); });
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }

    @Test
    @SmallTest
    public void testExitFullscreenUsingRunnable() {
        // Running the runnable supplied to the delegate should exit fullscreen.
        TestThreadUtils.runOnUiThreadBlocking(mDelegate.mExitFullscreenRunnable);
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }
}
