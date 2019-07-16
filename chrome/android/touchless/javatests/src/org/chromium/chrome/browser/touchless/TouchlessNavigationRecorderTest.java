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
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.suggestions.NavigationRecorder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link NavigationRecorder} run in the context of a SingleTabActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TouchlessNavigationRecorderTest {
    @Rule
    public ChromeActivityTestRule<NoTouchActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(NoTouchActivity.class);

    private EmbeddedTestServer mTestServer;
    private String mNavUrl;
    private Tab mInitialTab;

    private NoTouchActivity mActivity;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mNavUrl = mTestServer.getURL("/chrome/test/data/android/google.html");

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals(
                                mActivity.getActivityTab().getWebContents().getLastCommittedUrl(),
                                UrlConstants.NTP_URL));

        mInitialTab = mActivityTestRule.getActivity().getActivityTab();
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
    public void testRecordVisitOnDestroy() throws InterruptedException, TimeoutException {
        final CallbackHelper callback = new CallbackHelper();
        loadUrlAndRecordVisit(mNavUrl, (NavigationRecorder.VisitData visit) -> {
            // While we are navigation back to the NTP, NavigationRecorder always passed null as
            // the URL unless a 'back 'navigation is performed.
            Assert.assertEquals(null, visit.endUrl);
            callback.notifyCalled();
        });

        ChromeTabUtils.waitForTabPageLoaded(mInitialTab, (String) null);

        // This will trigger an intent and use PageTransition.FROM_API.
        mActivityTestRule.getActivity().getTabCreator(false).launchNTP();
        callback.waitForCallback(0);
    }

    /** Loads the provided URL in the current tab and sets up navigation recording for it. */
    private void loadUrlAndRecordVisit(
            final String url, Callback<NavigationRecorder.VisitData> visitCallback) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mInitialTab.loadUrl(new LoadUrlParams(url)); });
        NavigationRecorder.record(mInitialTab, visitCallback);
    }
}
