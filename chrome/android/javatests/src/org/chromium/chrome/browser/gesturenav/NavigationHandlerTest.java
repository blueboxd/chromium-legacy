// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.app.Activity;
import android.graphics.Point;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.DisplayMetrics;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests {@link NavigationHandler} navigating back/forward using overscroll history navigation.
 * TODO(jinsukkim): Add more tests (right swipe, tab switcher, etc).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=OverscrollHistoryNavigation"})
public class NavigationHandlerTest {
    private static final String RENDERED_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final boolean LEFT_EDGE = true;
    private static final boolean RIGHT_EDGE = false;
    private static final int PAGELOAD_TIMEOUT_MS = 4000;

    private EmbeddedTestServer mTestServer;
    private float mEdgeWidthPx;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        CompositorAnimationHandler.setTestingMode(true);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getMetrics(
                displayMetrics);
        mEdgeWidthPx = displayMetrics.density * NavigationHandler.EDGE_WIDTH_DP;
    }

    @After
    public void tearDown() {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
    }

    private Tab currentTab() {
        return mActivityTestRule.getActivity().getActivityTabProvider().get();
    }

    private void loadNewTabPage() {
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), false, true);
    }

    private void assertNavigateOnSwipeFrom(boolean edge, String toUrl) {
        ChromeTabUtils.waitForTabPageLoaded(currentTab(), toUrl, () -> swipeFromEdge(edge), 10);
        CriteriaHelper.pollUiThread(Criteria.equals(toUrl, () -> currentTab().getUrlString()));
        Assert.assertEquals("Didn't navigate back", toUrl, currentTab().getUrlString());
    }

    private void swipeFromEdge(boolean leftEdge) {
        Point size = new Point();
        mActivityTestRule.getActivity().getWindowManager().getDefaultDisplay().getSize(size);

        // Swipe from an edge toward the middle of the screen.
        float dragStartX = leftEdge ? mEdgeWidthPx / 2 : size.x - mEdgeWidthPx / 2;
        float dragEndX = size.x / 2;
        float dragStartY = size.y / 2;
        float dragEndY = size.y / 2;
        long downTime = SystemClock.uptimeMillis();

        TouchCommon.dragStart(mActivityTestRule.getActivity(), dragStartX, dragStartY, downTime);
        TouchCommon.dragTo(mActivityTestRule.getActivity(), dragStartX, dragEndX, dragStartY,
                dragEndY, /* stepCount= */ 100, downTime);
        TouchCommon.dragEnd(mActivityTestRule.getActivity(), dragEndX, dragEndY, downTime);
    }

    @Test
    @SmallTest
    public void testCloseChromeAtHistoryStackHead() {
        loadNewTabPage();
        final Activity activity = mActivityTestRule.getActivity();
        swipeFromEdge(LEFT_EDGE);
        CriteriaHelper.pollUiThread(() -> {
            int state = ApplicationStatus.getStateForActivity(activity);
            return state == ActivityState.STOPPED || state == ActivityState.DESTROYED;
        }, "Chrome should be in background");
    }

    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_greater_than = Build.VERSION_CODES.O_MR1, message = "Flaky on P crbug.com/1041233")
    public void testSwipeNavigateOnNativePage() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        assertNavigateOnSwipeFrom(LEFT_EDGE, UrlConstants.NTP_URL);
        assertNavigateOnSwipeFrom(RIGHT_EDGE, UrlConstants.RECENT_TABS_URL);
    }

    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_greater_than = Build.VERSION_CODES.O_MR1, message = "Flaky on P crbug.com/1041233")
    public void testSwipeNavigateOnRenderedPage() {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        mActivityTestRule.loadUrl(mTestServer.getURL(RENDERED_PAGE));
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        assertNavigateOnSwipeFrom(LEFT_EDGE, mTestServer.getURL(RENDERED_PAGE));
        // clang-format off
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            new Handler().postDelayed(() -> assertNavigateOnSwipeFrom(
                    RIGHT_EDGE, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL), PAGELOAD_TIMEOUT_MS);
        });
        // clang-format on
    }
}
