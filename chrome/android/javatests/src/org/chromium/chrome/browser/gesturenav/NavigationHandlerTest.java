// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.app.Activity;
import android.graphics.Point;
import android.os.Handler;
import android.support.test.InstrumentationRegistry;
import android.util.DisplayMetrics;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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
    private NavigationHandler mNavigationHandler;
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
        TabbedRootUiCoordinator uiCoordinator =
                (TabbedRootUiCoordinator) mActivityTestRule.getActivity()
                        .getRootUiCoordinatorForTesting();
        mNavigationHandler = uiCoordinator.getHistoryNavigationCoordinatorForTesting()
                                     .getNavigationHandlerForTesting();
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
        final int eventCounts = 100;
        final float startx = leftEdge ? mEdgeWidthPx / 2 : size.x - mEdgeWidthPx / 2;
        final float y = size.y / 2;
        final float step = (size.x / 2 - startx) / eventCounts;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mNavigationHandler.onDown();
            float endx = startx + step;
            for (int i = 0; i < eventCounts; i++, endx += step) {
                mNavigationHandler.onScroll(startx, -step, 0, endx, y);
            }
            mNavigationHandler.release(true);
        });
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
    public void testSwipeNavigateOnNativePage() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        assertNavigateOnSwipeFrom(LEFT_EDGE, UrlConstants.NTP_URL);
        assertNavigateOnSwipeFrom(RIGHT_EDGE, UrlConstants.RECENT_TABS_URL);
    }

    @Test
    @SmallTest
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
