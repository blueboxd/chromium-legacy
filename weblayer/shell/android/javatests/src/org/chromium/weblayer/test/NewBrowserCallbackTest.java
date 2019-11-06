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
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.NewBrowserCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that NewBrowserCallback methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class NewBrowserCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private InstrumentationActivity mActivity;

    private static class NewBrowserCallbackImpl extends NewBrowserCallback {
        public int mNewBrowserCount;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void onNewBrowser(BrowserController browser, int mode) {
            mCallbackHelper.notifyCalled();
            browser.getBrowserFragmentController().setActiveBrowserController(browser);
        }

        public void waitForNewBrowser() {
            try {
                mCallbackHelper.waitForCallback(0);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    @Before
    public void setUp() {
        mTestServer = new EmbeddedTestServer();
        mTestServer.initializeNative(InstrumentationRegistry.getInstrumentation().getContext(),
                EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mTestServer.addDefaultHandlers("weblayer/test/data");
        Assert.assertTrue(mTestServer.start(0));
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    public void testNewBrowser() {
        String url = mTestServer.getURL("/new_browser.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        NewBrowserCallbackImpl callback = new NewBrowserCallbackImpl();
        BrowserController firstBrowserController =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    BrowserController browserController =
                            mActivity.getBrowserFragmentController().getActiveBrowserController();
                    browserController.setNewBrowserCallback(callback);
                    return browserController;
                });

        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        callback.waitForNewBrowser();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    2, mActivity.getBrowserFragmentController().getBrowserControllers().size());
            BrowserController browserController =
                    mActivity.getBrowserFragmentController().getActiveBrowserController();
            Assert.assertNotSame(firstBrowserController, browserController);
        });
    }
}
