// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Instrumentation tests for {@link TouchlessNewTabPageMediator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchlessNewTabPageMediatorTest {
    @Rule
    public ChromeActivityTestRule<NoTouchActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(NoTouchActivity.class);

    private EmbeddedTestServer mTestServer;
    private Tab mInitialTab;
    private NavigationController mNavigationController;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityFromLauncher();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mInitialTab = mActivityTestRule.getActivity().getActivityTab();
        mNavigationController = mInitialTab.getWebContents().getNavigationController();

        // NTP is going to load by default.
        ChromeTabUtils.waitForTabPageLoaded(mInitialTab, (String) null);
    }

    @After
    public void tearDown() {
        // If setUp() fails, tearDown() still needs to be able to execute without exceptions.
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    public void testRendererPageLoad() throws InterruptedException {
        String rendererUrl = mTestServer.getURL("/chrome/test/data/android/google.html");
        verifyScrollPosition((String pos) -> Assert.assertEquals("", pos));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mInitialTab.loadUrl(new LoadUrlParams(rendererUrl)); });
        ChromeTabUtils.waitForTabPageLoaded(mInitialTab, (String) null);
        verifyScrollPosition((String pos) -> Assert.assertNotEquals("", pos));
    }

    @Test
    @SmallTest
    public void testNativePageLoad() throws InterruptedException {
        verifyScrollPosition((String pos) -> Assert.assertEquals("", pos));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mInitialTab.loadUrl(new LoadUrlParams(UrlConstants.EXPLORE_URL)); });
        ChromeTabUtils.waitForTabPageLoaded(mInitialTab, (String) null);
        verifyScrollPosition((String pos) -> Assert.assertNotEquals("", pos));
    }

    private void verifyScrollPosition(Callback<String> verification) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> verification.onResult(mNavigationController.getEntryExtraData(
                                0, "TouchlessScrollPosition")));
    }
}
